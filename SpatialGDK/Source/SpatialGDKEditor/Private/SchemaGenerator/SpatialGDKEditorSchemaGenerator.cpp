// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "SpatialGDKEditorSchemaGenerator.h"

#include "Abilities/GameplayAbility.h"
#include "AssetRegistryModule.h"
#include "Async/Async.h"
#include "Components/SceneComponent.h"
#include "Editor.h"
#include "Engine/LevelScriptActor.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "GeneralProjectSettings.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/MonitoredProcess.h"
#include "Templates/SharedPointer.h"
#include "UObject/UObjectIterator.h"

#include "Engine/WorldComposition.h"
#include "Interop/SpatialClassInfoManager.h"
#include "Misc/ScopedSlowTask.h"
#include "SchemaGenerator.h"
#include "Settings/ProjectPackagingSettings.h"
#include "SpatialConstants.h"
#include "SpatialGDKEditorSettings.h"
#include "SpatialGDKServicesModule.h"
#include "TypeStructure.h"
#include "UObject/StrongObjectPtr.h"
#include "Utils/CodeWriter.h"
#include "Utils/ComponentIdGenerator.h"
#include "Utils/DataTypeUtilities.h"
#include "Utils/SchemaDatabase.h"

DEFINE_LOG_CATEGORY(LogSpatialGDKSchemaGenerator);
#define LOCTEXT_NAMESPACE "SpatialGDKSchemaGenerator"

TArray<UClass*> SchemaGeneratedClasses;
TMap<FString, FActorSchemaData> ActorClassPathToSchema;
TMap<FString, FSubobjectSchemaData> SubobjectClassPathToSchema;
uint32 NextAvailableComponentId;

// LevelStreaming
TMap<FString, uint32> LevelPathToComponentId;
TSet<uint32> LevelComponentIds;

// Prevent name collisions.
TMap<FString, FString> ClassPathToSchemaName;
TMap<FString, FString> SchemaNameToClassPath;
TMap<FString, TSet<FString>> PotentialSchemaNameCollisions;

namespace
{

void AddPotentialNameCollision(const FString& DesiredSchemaName, const FString& ClassPath, const FString& GeneratedSchemaName)
{
	PotentialSchemaNameCollisions.FindOrAdd(DesiredSchemaName).Add(FString::Printf(TEXT("%s(%s)"), *ClassPath, *GeneratedSchemaName));
}

void OnStatusOutput(FString Message)
{
	UE_LOG(LogSpatialGDKSchemaGenerator, Log, TEXT("%s"), *Message);
}

void GenerateCompleteSchemaFromClass(FString SchemaPath, FComponentIdGenerator& IdGenerator, TSharedPtr<FUnrealType> TypeInfo)
{
	UClass* Class = Cast<UClass>(TypeInfo->Type);
	FString SchemaFilename = UnrealNameToSchemaName(Class->GetName());

	if (Class->IsChildOf<AActor>())
	{
		GenerateActorSchema(IdGenerator, Class, TypeInfo, SchemaPath);
	}
	else
	{
		GenerateSubobjectSchema(IdGenerator, Class, TypeInfo, SchemaPath + TEXT("Subobjects/"));
	}
}

bool CheckSchemaNameValidity(FString Name, FString Identifier, FString Category)
{
	if (Name.IsEmpty())
	{
		UE_LOG(LogSpatialGDKSchemaGenerator, Error, TEXT("%s %s is empty after removing non-alphanumeric characters, schema not generated."), *Category, *Identifier);
		return false;
	}

	if (FChar::IsDigit(Name[0]))
	{
		UE_LOG(LogSpatialGDKSchemaGenerator, Error, TEXT("%s names should not start with digits. %s %s (%s) has leading digits (potentially after removing non-alphanumeric characters), schema not generated."), *Category, *Category, *Name, *Identifier);
		return false;
	}

	return true;
}

void CheckIdentifierNameValidity(TSharedPtr<FUnrealType> TypeInfo, bool& bOutSuccess)
{
	// Check Replicated data.
	FUnrealFlatRepData RepData = GetFlatRepData(TypeInfo);
	for (EReplicatedPropertyGroup Group : GetAllReplicatedPropertyGroups())
	{
		TMap<FString, TSharedPtr<FUnrealProperty>> SchemaReplicatedDataNames;
		for (auto& RepProp : RepData[Group])
		{
			FString NextSchemaReplicatedDataName = SchemaFieldName(RepProp.Value);

			if (!CheckSchemaNameValidity(NextSchemaReplicatedDataName, RepProp.Value->Property->GetPathName(), TEXT("Replicated property")))
			{
				bOutSuccess = false;
			}

			if (TSharedPtr<FUnrealProperty>* ExistingReplicatedProperty = SchemaReplicatedDataNames.Find(NextSchemaReplicatedDataName))
			{
				UE_LOG(LogSpatialGDKSchemaGenerator, Error, TEXT("Replicated property name collision after removing non-alphanumeric characters, schema not generated. Name '%s' collides for '%s' and '%s'"),
					*NextSchemaReplicatedDataName, *ExistingReplicatedProperty->Get()->Property->GetPathName(), *RepProp.Value->Property->GetPathName());
				bOutSuccess = false;
			}
			else
			{
				SchemaReplicatedDataNames.Add(NextSchemaReplicatedDataName, RepProp.Value);
			}
		}
	}

	// Check Handover data.
	FCmdHandlePropertyMap HandoverData = GetFlatHandoverData(TypeInfo);
	TMap<FString, TSharedPtr<FUnrealProperty>> SchemaHandoverDataNames;
	for (auto& Prop : HandoverData)
	{
		FString NextSchemaHandoverDataName = SchemaFieldName(Prop.Value);

		if (!CheckSchemaNameValidity(NextSchemaHandoverDataName, Prop.Value->Property->GetPathName(), TEXT("Handover property")))
		{
			bOutSuccess = false;
		}

		if (TSharedPtr<FUnrealProperty>* ExistingHandoverData = SchemaHandoverDataNames.Find(NextSchemaHandoverDataName))
		{
			UE_LOG(LogSpatialGDKSchemaGenerator, Error, TEXT("Handover data name collision after removing non-alphanumeric characters, schema not generated. Name '%s' collides for '%s' and '%s'"),
				*NextSchemaHandoverDataName, *ExistingHandoverData->Get()->Property->GetPathName(), *Prop.Value->Property->GetPathName());
			bOutSuccess = false;
		}
		else
		{
			SchemaHandoverDataNames.Add(NextSchemaHandoverDataName, Prop.Value);
		}
	}

	// Check RPC name validity.
	FUnrealRPCsByType RPCsByType = GetAllRPCsByType(TypeInfo);
	for (auto Group : GetRPCTypes())
	{
		TMap<FString, TSharedPtr<FUnrealRPC>> SchemaRPCNames;
		for (auto& RPC : RPCsByType[Group])
		{
			FString NextSchemaRPCName = SchemaRPCName(RPC->Function);

			if (!CheckSchemaNameValidity(NextSchemaRPCName, RPC->Function->GetPathName(), TEXT("RPC")))
			{
				bOutSuccess = false;
			}

			if (TSharedPtr<FUnrealRPC>* ExistingRPC = SchemaRPCNames.Find(NextSchemaRPCName))
			{
				UE_LOG(LogSpatialGDKSchemaGenerator, Error, TEXT("RPC name collision after removing non-alphanumeric characters, schema not generated. Name '%s' collides for '%s' and '%s'"),
					*NextSchemaRPCName, *ExistingRPC->Get()->Function->GetPathName(), *RPC->Function->GetPathName());
				bOutSuccess = false;
			}
			else
			{
				SchemaRPCNames.Add(NextSchemaRPCName, RPC);
			}
		}
	}

	// Check subobject name validity.
	FSubobjectMap Subobjects = GetAllSubobjects(TypeInfo);
	TMap<FString, TSharedPtr<FUnrealType>> SchemaSubobjectNames;
	for (auto& It : Subobjects)
	{
		TSharedPtr<FUnrealType>& SubobjectTypeInfo = It.Value;
		FString NextSchemaSubobjectName = UnrealNameToSchemaComponentName(SubobjectTypeInfo->Name.ToString());

		if (!CheckSchemaNameValidity(NextSchemaSubobjectName, SubobjectTypeInfo->Object->GetPathName(), TEXT("Subobject")))
		{
			bOutSuccess = false;
		}

		if (TSharedPtr<FUnrealType>* ExistingSubobject = SchemaSubobjectNames.Find(NextSchemaSubobjectName))
		{
			UE_LOG(LogSpatialGDKSchemaGenerator, Error, TEXT("Subobject name collision after removing non-alphanumeric characters, schema not generated. Name '%s' collides for '%s' and '%s'"),
				*NextSchemaSubobjectName, *ExistingSubobject->Get()->Object->GetPathName(), *SubobjectTypeInfo->Object->GetPathName());
			bOutSuccess = false;
		}
		else
		{
			SchemaSubobjectNames.Add(NextSchemaSubobjectName, SubobjectTypeInfo);
		}
	}
}

bool ValidateIdentifierNames(TArray<TSharedPtr<FUnrealType>>& TypeInfos)
{
	bool bSuccess = true;

	// Remove all underscores from the class names, check for duplicates or invalid schema names.
	for (const auto& TypeInfo : TypeInfos)
	{
		UClass* Class = Cast<UClass>(TypeInfo->Type);
		check(Class);
		const FString& ClassName = Class->GetName();
		const FString& ClassPath = Class->GetPathName();
		FString SchemaName = UnrealNameToSchemaName(ClassName);

		if (!CheckSchemaNameValidity(SchemaName, ClassPath, TEXT("Class")))
		{
			bSuccess = false;
		}

		FString DesiredSchemaName = SchemaName;

		if (ClassPathToSchemaName.Contains(ClassPath))
		{
			continue;
		}

		int Suffix = 0;
		while (SchemaNameToClassPath.Contains(SchemaName))
		{
			SchemaName = UnrealNameToSchemaName(ClassName) + FString::Printf(TEXT("%d"), ++Suffix);
		}

		ClassPathToSchemaName.Add(ClassPath, SchemaName);
		SchemaNameToClassPath.Add(SchemaName, ClassPath);

		if (DesiredSchemaName != SchemaName)
		{
			AddPotentialNameCollision(DesiredSchemaName, ClassPath, SchemaName);
		}
		AddPotentialNameCollision(SchemaName, ClassPath, SchemaName);
	}

	for (const auto& Collision : PotentialSchemaNameCollisions)
	{
		if (Collision.Value.Num() > 1)
		{
			UE_LOG(LogSpatialGDKSchemaGenerator, Warning, TEXT("Class name collision after removing non-alphanumeric characters. Name '%s' collides for classes [%s]"),
				*Collision.Key, *FString::Join(Collision.Value, TEXT(", ")));
		}
	}

	// Check for invalid/duplicate names in the generated type info.
	for (auto& TypeInfo : TypeInfos)
	{
		CheckIdentifierNameValidity(TypeInfo, bSuccess);
	}

	return bSuccess;
}

}// ::

void GenerateSchemaFromClasses(const TArray<TSharedPtr<FUnrealType>>& TypeInfos, const FString& CombinedSchemaPath, FComponentIdGenerator& IdGenerator)
{
	// Generate the actual schema.
	FScopedSlowTask Progress((float)TypeInfos.Num(), LOCTEXT("GenerateSchemaFromClasses", "Generating Schema..."));
	for (const auto& TypeInfo : TypeInfos)
	{
		Progress.EnterProgressFrame(1.f);
		GenerateCompleteSchemaFromClass(CombinedSchemaPath, IdGenerator, TypeInfo);
	}
}

void WriteLevelComponent(FCodeWriter& Writer, FString LevelName, uint32 ComponentId)
{
	Writer.PrintNewLine();
	Writer.Printf("component {0} {", *UnrealNameToSchemaComponentName(LevelName));
	Writer.Indent();
	Writer.Printf("id = {0};", ComponentId);
	Writer.Outdent().Print("}");
}

void GenerateSchemaForSublevels(const FString& SchemaPath, FComponentIdGenerator& IdGenerator)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	TArray<FAssetData> WorldAssets;
	AssetRegistryModule.Get().GetAllAssets(WorldAssets, true);

	// Filter assets to game maps.
	WorldAssets = WorldAssets.FilterByPredicate([](FAssetData Data) {
		return (Data.AssetClass == UWorld::StaticClass()->GetFName() && Data.PackagePath.ToString().StartsWith("/Game"));
	});

	TMultiMap<FName, FName> LevelNamesToPaths;

	for (FAssetData World : WorldAssets)
	{
		LevelNamesToPaths.Add(World.AssetName, World.PackageName);
	}

	FCodeWriter Writer;
	Writer.Printf(R"""(
		// Copyright (c) Improbable Worlds Ltd, All Rights Reserved
		// Note that this file has been generated automatically
		package unreal.sublevels;)""");

	TArray<FName> Keys;
	LevelNamesToPaths.GetKeys(Keys);

	for (FName LevelName : Keys)
	{
		if (LevelNamesToPaths.Num(LevelName) > 1)
		{
			// Write multiple numbered components.
			TArray<FName> LevelPaths;
			LevelNamesToPaths.MultiFind(LevelName, LevelPaths);
			FString LevelNameString = LevelName.ToString();

			for (int i = 0; i < LevelPaths.Num(); i++)
			{
				uint32 ComponentId = LevelPathToComponentId.FindRef(LevelPaths[i].ToString());
				if (ComponentId == 0)
				{
					ComponentId = IdGenerator.Next();
					LevelPathToComponentId.Add(LevelPaths[i].ToString(), ComponentId);
					LevelComponentIds.Add(ComponentId);
				}
				WriteLevelComponent(Writer, FString::Printf(TEXT("%s%d"), *LevelNameString, i), ComponentId);
				
			}
		}
		else
		{
			// Write a single component.
			FString LevelPath = LevelNamesToPaths.FindRef(LevelName).ToString();
			uint32 ComponentId = LevelPathToComponentId.FindRef(LevelPath);
			if (ComponentId == 0)
			{
				ComponentId = IdGenerator.Next();
				LevelPathToComponentId.Add(LevelPath, ComponentId);
				LevelComponentIds.Add(ComponentId);
			}
			WriteLevelComponent(Writer, LevelName.ToString(), ComponentId);
		}
	}

	Writer.WriteToFile(FString::Printf(TEXT("%sSublevels/sublevels.schema"), *SchemaPath));
}

FString GenerateIntermediateDirectory()
{
	const FString CombinedIntermediatePath = FPaths::Combine(*FPaths::GetPath(FPaths::GetProjectFilePath()), TEXT("Intermediate/Improbable/"), *FGuid::NewGuid().ToString(), TEXT("/"));
	FString AbsoluteCombinedIntermediatePath = FPaths::ConvertRelativePathToFull(CombinedIntermediatePath);
	FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*AbsoluteCombinedIntermediatePath);

	return AbsoluteCombinedIntermediatePath;
}

TMap<uint32, FString> CreateComponentIdToClassPathMap()
{
	TMap<uint32, FString> ComponentIdToClassPath;

	for (const auto& ActorSchemaData : ActorClassPathToSchema)
	{
		ForAllSchemaComponentTypes([&](ESchemaComponentType Type)
		{
			ComponentIdToClassPath.Add(ActorSchemaData.Value.SchemaComponents[Type], ActorSchemaData.Key);
		});

		for (const auto& SubobjectSchemaData : ActorSchemaData.Value.SubobjectData)
		{
			ForAllSchemaComponentTypes([&](ESchemaComponentType Type)
			{
				ComponentIdToClassPath.Add(SubobjectSchemaData.Value.SchemaComponents[Type], SubobjectSchemaData.Value.ClassPath);
			});
		}
	}

	for (const auto& SubobjectSchemaData : SubobjectClassPathToSchema)
	{
		for (const auto& DynamicSubobjectData : SubobjectSchemaData.Value.DynamicSubobjectComponents)
		{
			ForAllSchemaComponentTypes([&](ESchemaComponentType Type)
			{
				ComponentIdToClassPath.Add(DynamicSubobjectData.SchemaComponents[Type], SubobjectSchemaData.Key);
			});
		}
	}

	ComponentIdToClassPath.Remove(SpatialConstants::INVALID_COMPONENT_ID);

	return ComponentIdToClassPath;
}

void SaveSchemaDatabase()
{
	FString PackagePath = TEXT("/Game/Spatial/SchemaDatabase");
	UPackage *Package = CreatePackage(nullptr, *PackagePath);

	USchemaDatabase* SchemaDatabase = NewObject<USchemaDatabase>(Package, USchemaDatabase::StaticClass(), FName("SchemaDatabase"), EObjectFlags::RF_Public | EObjectFlags::RF_Standalone);
	SchemaDatabase->NextAvailableComponentId = NextAvailableComponentId;
	SchemaDatabase->ActorClassPathToSchema = ActorClassPathToSchema;
	SchemaDatabase->SubobjectClassPathToSchema = SubobjectClassPathToSchema;
	SchemaDatabase->LevelPathToComponentId = LevelPathToComponentId;
	SchemaDatabase->ComponentIdToClassPath = CreateComponentIdToClassPathMap();
	SchemaDatabase->LevelComponentIds = LevelComponentIds;

	FAssetRegistryModule::AssetCreated(SchemaDatabase);
	SchemaDatabase->MarkPackageDirty();

	// NOTE: UPackage::GetMetaData() has some code where it will auto-create the metadata if it's missing
	// UPackage::SavePackage() calls UPackage::GetMetaData() at some point, and will cause an exception to get thrown
	// if the metadata auto-creation branch needs to be taken. This is the case when generating the schema from the
	// command line, so we just pre-empt it here.
	Package->GetMetaData();

	FString FilePath = FString::Printf(TEXT("%s%s"), *PackagePath, *FPackageName::GetAssetPackageExtension());
	bool bSuccess = UPackage::SavePackage(Package, SchemaDatabase, EObjectFlags::RF_Public | EObjectFlags::RF_Standalone, *FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension()));

	if (!bSuccess)
	{
		FString FullPath = FPaths::ConvertRelativePathToFull(FilePath);
		FPaths::MakePlatformFilename(FullPath);
		FMessageDialog::Debugf(FText::FromString(FString::Printf(TEXT("Unable to save Schema Database to '%s'! Please make sure the file is writeable."), *FullPath)));
	}
}

TArray<UClass*> GetAllSupportedClasses()
{
	TSet<UClass*> Classes;
	const TArray<FDirectoryPath>& DirectoriesToNeverCook = GetDefault<UProjectPackagingSettings>()->DirectoriesToNeverCook;

	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		// User told us to ignore this class
		if (ClassIt->HasAnySpatialClassFlags(SPATIALCLASS_NotSpatialType))
		{
			continue;
		}

		UClass* SupportedClass = *ClassIt;

		// Ensure we don't process transient generated classes for BP
		if (SupportedClass->GetName().StartsWith(TEXT("SKEL_"), ESearchCase::CaseSensitive)
			|| SupportedClass->GetName().StartsWith(TEXT("REINST_"), ESearchCase::CaseSensitive)
			|| SupportedClass->GetName().StartsWith(TEXT("TRASHCLASS_"), ESearchCase::CaseSensitive)
			|| SupportedClass->GetName().StartsWith(TEXT("HOTRELOADED_"), ESearchCase::CaseSensitive)
			|| SupportedClass->GetName().StartsWith(TEXT("PROTO_BP_"), ESearchCase::CaseSensitive)
			|| SupportedClass->GetName().StartsWith(TEXT("PLACEHOLDER-CLASS_"), ESearchCase::CaseSensitive)
			|| SupportedClass->GetName().StartsWith(TEXT("ORPHANED_DATA_ONLY_"), ESearchCase::CaseSensitive))
		{
			continue;
		}

		// Avoid processing classes contained in Directories to Never Cook
		const FString& ClassPath = SupportedClass->GetPathName();
		if (DirectoriesToNeverCook.ContainsByPredicate([&ClassPath](const FDirectoryPath& Directory)
		{
			return ClassPath.StartsWith(Directory.Path);
		}))
		{
			continue;
		}

		Classes.Add(SupportedClass);
	}

	return Classes.Array();
}

void CopyWellKnownSchemaFiles()
{
	FString PluginDir = GetDefault<USpatialGDKEditorSettings>()->GetGDKPluginDirectory();

	FString GDKSchemaDir = FPaths::Combine(PluginDir, TEXT("SpatialGDK/Extras/schema"));
	FString GDKSchemaCopyDir = FPaths::Combine(FSpatialGDKServicesModule::GetSpatialOSDirectory(), TEXT("schema/unreal/gdk"));

	FString CoreSDKSchemaDir = FPaths::Combine(PluginDir, TEXT("SpatialGDK/Binaries/ThirdParty/Improbable/Programs/schema"));
	FString CoreSDKSchemaCopyDir = FPaths::Combine(FSpatialGDKServicesModule::GetSpatialOSDirectory(), TEXT("build/dependencies/schema/standard_library"));
	
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (!PlatformFile.DirectoryExists(*GDKSchemaCopyDir))
	{
		if (!PlatformFile.CreateDirectoryTree(*GDKSchemaCopyDir))
		{
			UE_LOG(LogSpatialGDKSchemaGenerator, Error, TEXT("Could not create gdk schema directory '%s'! Please make sure the parent directory is writeable."), *GDKSchemaCopyDir);
		}
	}

	if (!PlatformFile.DirectoryExists(*CoreSDKSchemaCopyDir))
	{
		if (!PlatformFile.CreateDirectoryTree(*CoreSDKSchemaCopyDir))
		{
			UE_LOG(LogSpatialGDKSchemaGenerator, Error, TEXT("Could not create standard library schema directory '%s'! Please make sure the parent directory is writeable."), *GDKSchemaCopyDir);
		}
	}

	if (!PlatformFile.CopyDirectoryTree(*GDKSchemaCopyDir, *GDKSchemaDir, true /*bOverwriteExisting*/))
	{
		UE_LOG(LogSpatialGDKSchemaGenerator, Error, TEXT("Could not copy gdk schema to '%s'! Please make sure the directory is writeable."), *GDKSchemaCopyDir);
	}

	if (!PlatformFile.CopyDirectoryTree(*CoreSDKSchemaCopyDir, *CoreSDKSchemaDir, true /*bOverwriteExisting*/))
	{
		UE_LOG(LogSpatialGDKSchemaGenerator, Error, TEXT("Could not copy standard library schema to '%s'! Please make sure the directory is writeable."), *CoreSDKSchemaCopyDir);
	}
}

void DeleteGeneratedSchemaFiles()
{
	const FString SchemaOutputPath = GetDefault<USpatialGDKEditorSettings>()->GetGeneratedSchemaOutputFolder();
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (PlatformFile.DirectoryExists(*SchemaOutputPath))
	{
		if (!PlatformFile.DeleteDirectoryRecursively(*SchemaOutputPath))
		{
			UE_LOG(LogSpatialGDKSchemaGenerator, Error, TEXT("Could not clean the generated schema directory '%s'! Please make sure the directory and the files inside are writeable."), *SchemaOutputPath);
		}
	}
	PlatformFile.CreateDirectory(*SchemaOutputPath);
}

void ClearGeneratedSchema()
{
	ActorClassPathToSchema.Empty();
	SubobjectClassPathToSchema.Empty();
	LevelComponentIds.Empty();
	LevelPathToComponentId.Empty();
	NextAvailableComponentId = SpatialConstants::STARTING_GENERATED_COMPONENT_ID;

	// As a safety precaution, if the SchemaDatabase.uasset doesn't exist then make sure the schema generated folder is cleared as well.
	DeleteGeneratedSchemaFiles();
}

bool TryLoadExistingSchemaDatabase()
{
	const FString SchemaDatabasePackagePath = TEXT("/Game/Spatial/SchemaDatabase");
	const FString SchemaDatabaseAssetPath = FString::Printf(TEXT("%s.SchemaDatabase"), *SchemaDatabasePackagePath);
	const FString SchemaDatabaseFileName = FPackageName::LongPackageNameToFilename(SchemaDatabasePackagePath, FPackageName::GetAssetPackageExtension());

	FFileStatData StatData = FPlatformFileManager::Get().GetPlatformFile().GetStatData(*SchemaDatabaseFileName);

	if (StatData.bIsValid)
	{
		if (StatData.bIsReadOnly)
		{
			UE_LOG(LogSpatialGDKSchemaGenerator, Error, TEXT("Schema Generation failed: Schema Database at %s%s is read only. Make it writable before generating schema"), *SchemaDatabasePackagePath, *FPackageName::GetAssetPackageExtension());
			return false;
		}

		const USchemaDatabase* const SchemaDatabase = Cast<USchemaDatabase>(FSoftObjectPath(SchemaDatabaseAssetPath).TryLoad());

		if (SchemaDatabase == nullptr)
		{
			UE_LOG(LogSpatialGDKSchemaGenerator, Error, TEXT("Schema Generation failed: Failed to load existing schema database."));
			return false;
		}

		ActorClassPathToSchema = SchemaDatabase->ActorClassPathToSchema;
		SubobjectClassPathToSchema = SchemaDatabase->SubobjectClassPathToSchema;
		LevelComponentIds = SchemaDatabase->LevelComponentIds;
		LevelPathToComponentId = SchemaDatabase->LevelPathToComponentId;
		NextAvailableComponentId = SchemaDatabase->NextAvailableComponentId;

		// Component Id generation was updated to be non-destructive, if we detect an old schema database, delete it.
		if (ActorClassPathToSchema.Num() > 0 && NextAvailableComponentId == SpatialConstants::STARTING_GENERATED_COMPONENT_ID)
		{
			UE_LOG(LogSpatialGDKSchemaGenerator, Warning, TEXT("Detected an old schema database, it'll be reset."));
			ClearGeneratedSchema();
		}
	}
	else
	{
		UE_LOG(LogSpatialGDKSchemaGenerator, Display, TEXT("SchemaDatabase not found so the generated schema directory will be cleared out if it exists."));
		ClearGeneratedSchema();
	}

	return true;
}

SPATIALGDKEDITOR_API bool GeneratedSchemaFolderExists()
{
	const FString SchemaOutputPath = GetDefault<USpatialGDKEditorSettings>()->GetGeneratedSchemaOutputFolder();
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	return PlatformFile.DirectoryExists(*SchemaOutputPath);
}

void ResolveClassPathToSchemaName(const FString& ClassPath, const FString& SchemaName)
{
	if (SchemaName.IsEmpty())
	{
		return;
	}

	ClassPathToSchemaName.Add(ClassPath, SchemaName);
	SchemaNameToClassPath.Add(SchemaName, ClassPath);
	FSoftObjectPath ObjPath = FSoftObjectPath(ClassPath);
	FString DesiredSchemaName = UnrealNameToSchemaName(ObjPath.GetAssetName());

	if (DesiredSchemaName != SchemaName)
	{
		AddPotentialNameCollision(DesiredSchemaName, ClassPath, SchemaName);
	}
	AddPotentialNameCollision(SchemaName, ClassPath, SchemaName);
}

void ResetUsedNames()
{
	ClassPathToSchemaName.Empty();
	SchemaNameToClassPath.Empty();
	PotentialSchemaNameCollisions.Empty();

	for (const TPair<FString, FActorSchemaData>& Entry : ActorClassPathToSchema)
	{
		ResolveClassPathToSchemaName(Entry.Key, Entry.Value.GeneratedSchemaName);
	}

 	for (const TPair< FString, FSubobjectSchemaData>& Entry : SubobjectClassPathToSchema)
 	{
		ResolveClassPathToSchemaName(Entry.Key, Entry.Value.GeneratedSchemaName);
 	}
}

void RunSchemaCompiler()
{
	FString PluginDir = GetDefault<USpatialGDKEditorSettings>()->GetGDKPluginDirectory();

	// Get the schema_compiler path and arguments
	FString SchemaCompilerExe = FPaths::Combine(PluginDir, TEXT("SpatialGDK/Binaries/ThirdParty/Improbable/Programs/schema_compiler.exe"));

	FString SchemaDir = FPaths::Combine(FSpatialGDKServicesModule::GetSpatialOSDirectory(), TEXT("schema"));
	FString CoreSDKSchemaDir = FPaths::Combine(FSpatialGDKServicesModule::GetSpatialOSDirectory(), TEXT("build/dependencies/schema/standard_library"));
	FString SchemaDescriptorDir = FPaths::Combine(FSpatialGDKServicesModule::GetSpatialOSDirectory(), TEXT("build/assembly/schema"));
	FString SchemaDescriptorOutput = FPaths::Combine(SchemaDescriptorDir, TEXT("schema.descriptor"));

	// The schema_compiler cannot create folders.
	if (!FPaths::DirectoryExists(SchemaDescriptorDir))
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.CreateDirectoryTree(*SchemaDescriptorDir);
	}

	FString SchemaCompilerArgs = FString::Printf(TEXT("--schema_path=\"%s\" --schema_path=\"%s\" --descriptor_set_out=\"%s\" --load_all_schema_on_schema_path"), *SchemaDir, *CoreSDKSchemaDir, *SchemaDescriptorOutput);

	UE_LOG(LogSpatialGDKSchemaGenerator, Log, TEXT("Starting '%s' with `%s` arguments."), *SchemaCompilerExe, *SchemaCompilerArgs);

	int32 ExitCode = 1;
	FString SchemaCompilerOut;
	FString SchemaCompilerErr;
	FPlatformProcess::ExecProcess(*SchemaCompilerExe, *SchemaCompilerArgs, &ExitCode, &SchemaCompilerOut, &SchemaCompilerErr);

	if (ExitCode == 0)
	{
		UE_LOG(LogSpatialGDKSchemaGenerator, Log, TEXT("schema_compiler successfully generated schema descriptor: %s"), *SchemaCompilerOut);
	}
	else
	{
		UE_LOG(LogSpatialGDKSchemaGenerator, Error, TEXT("schema_compiler failed to generate schema descriptor: %s"), *SchemaCompilerErr);
	}
}

bool SpatialGDKGenerateSchema()
{
	ResetUsedNames();

	// Gets the classes currently loaded into memory.
	SchemaGeneratedClasses = GetAllSupportedClasses();
	SchemaGeneratedClasses.Sort();

	// Generate Type Info structs for all classes
	TArray<TSharedPtr<FUnrealType>> TypeInfos;

	for (const auto& Class : SchemaGeneratedClasses)
	{
		// Parent and static array index start at 0 for checksum calculations.
		TypeInfos.Add(CreateUnrealTypeInfo(Class, 0, 0, false));
	}

	if (!ValidateIdentifierNames(TypeInfos))
	{
		return false;
	}

	FString SchemaOutputPath = GetDefault<USpatialGDKEditorSettings>()->GetGeneratedSchemaOutputFolder();

	UE_LOG(LogSpatialGDKSchemaGenerator, Display, TEXT("Schema path %s"), *SchemaOutputPath);

	// Check schema path is valid.
	if (!FPaths::CollapseRelativeDirectories(SchemaOutputPath))
	{
		UE_LOG(LogSpatialGDKSchemaGenerator, Error, TEXT("Invalid path: '%s'. Schema not generated."), *SchemaOutputPath);
		return false;
	}

	check(GetDefault<UGeneralProjectSettings>()->bSpatialNetworking);

	FComponentIdGenerator IdGenerator = FComponentIdGenerator(NextAvailableComponentId);

	GenerateSchemaFromClasses(TypeInfos, SchemaOutputPath, IdGenerator);
	GenerateSchemaForSublevels(SchemaOutputPath, IdGenerator);
	NextAvailableComponentId = IdGenerator.Peek();
	SaveSchemaDatabase();
	RunSchemaCompiler();

	return true;
}

#undef LOCTEXT_NAMESPACE
