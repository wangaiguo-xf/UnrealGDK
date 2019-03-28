// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "SpatialGDKEditor.h"

#include "Async/Async.h"
#include "Engine/WorldComposition.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/LevelScriptActor.h"

#include "SpatialGDKEditorSchemaGenerator.h"
#include "SpatialGDKEditorSnapshotGenerator.h"
#include "SpatialGDKEditorSettings.h"

#include "Engine/ObjectLibrary.h"

#include "Editor.h"
#include "Engine/AssetManager.h"
#include "EditorLevelUtils.h"
#include "Engine/LevelStreamingKismet.h"

#include "AssetRegistryModule.h"
#include "GeneralProjectSettings.h"

#include "ScopedSlowTask.h"

DEFINE_LOG_CATEGORY(LogSpatialGDKEditor);

FSpatialGDKEditor::FSpatialGDKEditor()
	: bSchemaGeneratorRunning(false)
{
	TryLoadExistingSchemaDatabase();
}

// This callback is copied from UEditorEngine so that we can turn it off during schema gen in editor.
void FSpatialGDKEditor::OnAssetLoaded(UObject* Asset)
{
	// do not init worlds when running schema gen.
	if (bSchemaGeneratorRunning)
	{
		//UE_LOG(LogTemp, Log, TEXT("OnAssetLoaded %s but schema generating so ignoring."), *GetFullNameSafe(Asset));
		return;
	}

	UWorld* World = Cast<UWorld>(Asset);
	if (World)
	{
		// Init inactive worlds here instead of UWorld::PostLoad because it is illegal to call UpdateWorldComponents while IsRoutingPostLoad
		check(World);
		if (!World->bIsWorldInitialized && World->WorldType == EWorldType::Inactive)
		{
			// Create the world without a physics scene because creating too many physics scenes causes deadlock issues in PhysX. The scene will be created when it is opened in the level editor.
			// Also, don't create an FXSystem because it consumes too much video memory. This is also created when the level editor opens this world.
			World->InitWorld(UWorld::InitializationValues()
				.ShouldSimulatePhysics(false)
				.EnableTraceCollision(true)
				.CreatePhysicsScene(false)
				.CreateFXSystem(false)
			);

			// Update components so the scene is populated
			World->UpdateWorldComponents(true, true);
		}
	}
}

void FSpatialGDKEditor::GenerateSchema(FSimpleDelegate SuccessCallback, FSimpleDelegate FailureCallback, FSpatialGDKEditorErrorHandler ErrorCallback)
{
	if (bSchemaGeneratorRunning)
	{
		UE_LOG(LogSpatialGDKEditor, Warning, TEXT("Schema generation is already running"));
		return;
	}

	bSchemaGeneratorRunning = true;

	// Force spatial networking so schema layouts are correct
	UGeneralProjectSettings* GeneralProjectSettings = GetMutableDefault<UGeneralProjectSettings>();
	bool bCachedSpatialNetworking = GeneralProjectSettings->bSpatialNetworking;
	GeneralProjectSettings->bSpatialNetworking = true;

	TryLoadExistingSchemaDatabase();

	const USpatialGDKEditorSettings* SpatialGDKSettings = GetDefault<USpatialGDKEditorSettings>();

	PreProcessSchemaMap();

	// Compile all dirty blueprints
	TArray<UBlueprint*> ErroredBlueprints;
	bool bPromptForCompilation = false;
	UEditorEngine::ResolveDirtyBlueprints(bPromptForCompilation, ErroredBlueprints);

	//LoadDefaultGameModes();

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	UObjectLibrary& Library = *UObjectLibrary::CreateLibrary(UObject::StaticClass(), true, false);

	if (UEditorEngine* EdEngine = Cast<UEditorEngine>(GEngine))
	{
		UE_LOG(LogTemp, Log, TEXT("Removing UEditorEngine::OnAssetLoaded."));
		FCoreUObjectDelegates::OnAssetLoaded.RemoveAll(EdEngine);
		if (!OnAssetLoadedHandle.IsValid())
		{
			UE_LOG(LogTemp, Log, TEXT("Replacing UEditorEngine::OnAssetLoaded with spatial version that won't run during schema gen."));
			FCoreUObjectDelegates::OnAssetLoaded.AddLambda([this](UObject* Asset) {
				OnAssetLoaded(Asset);
			});
		}
	}

	UE_LOG(LogSpatialGDKSchemaGenerator, Display, TEXT("Finding Assets To Load"));

	TArray<FAssetData> Assets;

	AssetRegistryModule.Get().GetAllAssets(Assets, true);

	// Filter assets to blueprint classes that are not loaded.
	Assets = Assets.FilterByPredicate([](FAssetData Data) {
		return (!Data.IsAssetLoaded() && Data.TagsAndValues.Contains("GeneratedClass") && Data.PackagePath.ToString().StartsWith("/Game"));
	});

	UE_LOG(LogSpatialGDKSchemaGenerator, Display, TEXT("Found %d assets to load."), Assets.Num());

	TArray<TStrongObjectPtr<UObject>> AssetPointers;
	FScopedSlowTask LoadAssetsProgress((float)Assets.Num() + 3.f, FText::FromString(FString::Printf(TEXT("Loading %d Assets before generating schema"), Assets.Num())));
	LoadAssetsProgress.MakeDialog(true);

	for (FAssetData Data : Assets)
	{
		if (LoadAssetsProgress.ShouldCancel()) {
			FailureCallback.ExecuteIfBound();
			bSchemaGeneratorRunning = false;
			return;
		}
		LoadAssetsProgress.EnterProgressFrame(1, FText::FromString(FString::Printf(TEXT("Loading %s"), *Data.AssetName.ToString())));
		if (auto GeneratedClassPathPtr = Data.TagsAndValues.Find("GeneratedClass"))
		{
			//UE_LOG(LogTemp, Log, TEXT("Loading %s"), *Data.GetFullName());
			const FString ClassObjectPath = FPackageName::ExportTextPathToObjectPath(*GeneratedClassPathPtr);
			const FString ClassName = FPackageName::ObjectPathToObjectName(ClassObjectPath);
			FSoftObjectPath SoftPath = FSoftObjectPath(ClassObjectPath);
			AssetPointers.Add(TStrongObjectPtr<UObject>(SoftPath.TryLoad()));
			//UE_LOG(LogTemp, Log, TEXT("Loaded %s ClassPath: %s, ClassName: %s"), *Data.GetFullName(), *ClassObjectPath, *ClassName);
		}
	}

	UE_LOG(LogSpatialGDKSchemaGenerator, Display, TEXT("Do Schema Gen"));
	LoadAssetsProgress.EnterProgressFrame(1, FText::FromString(FString::Printf(TEXT("Generating Schema"))));
	bool bResult = SpatialGDKGenerateSchema();
	UE_LOG(LogSpatialGDKSchemaGenerator, Display, TEXT("Do Schema Gen: done"));

	UE_LOG(LogSpatialGDKSchemaGenerator, Display, TEXT("Triggering GC"));
	LoadAssetsProgress.EnterProgressFrame(1, FText::FromString(FString::Printf(TEXT("Running Garbage Collection"))));
	AssetPointers.Empty();
	Assets.Empty();
	CollectGarbage(RF_NoFlags);
	UE_LOG(LogSpatialGDKSchemaGenerator, Display, TEXT("Triggering GC: done."));

	if (bResult)
	{
		SuccessCallback.ExecuteIfBound();
	}
	else
	{
		FailureCallback.ExecuteIfBound();
	}

	GetMutableDefault<UGeneralProjectSettings>()->bSpatialNetworking = bCachedSpatialNetworking;
	bSchemaGeneratorRunning = false;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         
}

void FSpatialGDKEditor::GenerateSnapshot(UWorld* World, FString SnapshotFilename, FSimpleDelegate SuccessCallback, FSimpleDelegate FailureCallback, FSpatialGDKEditorErrorHandler ErrorCallback)
{
	const bool bSuccess = SpatialGDKGenerateSnapshot(World, SnapshotFilename);

	if (bSuccess)
	{
		SuccessCallback.ExecuteIfBound();
	}
	else
	{
		FailureCallback.ExecuteIfBound();
	}
}

void FSpatialGDKEditor::UnloadLevels(TArray<ULevelStreaming*> LoadedLevels)
{
	for (ULevelStreaming* Level : LoadedLevels)
	{
		if (Level->HasLoadedLevel())
		{
			bool Success = EditorLevelUtils::RemoveLevelFromWorld(Level->GetLoadedLevel());
			UE_LOG(LogSpatialGDKEditor, Display, TEXT("Unloading %s : %s"), *GetPathNameSafe(Level), Success ? TEXT("Success") : TEXT("Failure"));
		}
		else
		{
			UE_LOG(LogSpatialGDKEditor, Display, TEXT("%s has no loaded level, skipping"), *GetPathNameSafe(Level));
		}
	}
}

TArray<ULevelStreaming*> FSpatialGDKEditor::LoadAllStreamingLevels(UWorld* World)
{
	/*const TArray<ULevelStreaming*> StreamingLevels = World->GetStreamingLevels();
	UE_LOG(LogSpatialGDKEditor, Display, TEXT("Loading %d Streaming SubLevels"), StreamingLevels.Num());
	for (ULevelStreaming* StreamingLevel : StreamingLevels)
	{
		StreamingLevel->SetShouldBeVisible(true);
		StreamingLevel->SetShouldBeVisibleInEditor(false);
		StreamingLevel->bShouldBlockOnLoad = true;
		World->AddStreamingLevel(StreamingLevel);
	}*/

	{
		UE_LOG(LogSpatialGDKEditor, Display, TEXT("Loading All Assets"));
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

		TArray<ULevelStreaming*> StreamingTiles = World->WorldComposition->TilesStreaming;
		TArray<FName> StreamingDependencies;

		for (ULevelStreaming* Tile : StreamingTiles)
		{
			UE_LOG(LogSpatialGDKEditor, Display, TEXT("Get Deps for %s"), *GetPathNameSafe(Tile));
			AssetRegistryModule.Get().GetDependencies(Tile->GetWorldAssetPackageFName(), StreamingDependencies);
		}

		TArray<FAssetData> AssetData;

		for (FName Package : StreamingDependencies)
		{
			AssetRegistryModule.Get().GetAssetsByPackageName(Package, AssetData, true);
		}

		UE_LOG(LogSpatialGDKEditor, Display, TEXT("Got %d Assets from maps:"), AssetData.Num());

		for (const FAssetData& Data : AssetData)
		{
			UE_LOG(LogSpatialGDKEditor, Display, TEXT("%s:"), *Data.AssetName.ToString());

			if (Data.TagsAndValues.Contains("GeneratedClass") && !Data.IsAssetLoaded())
			{
				UE_LOG(LogSpatialGDKEditor, Display, TEXT("Loading Blueprint Asset %s before schema gen"), *Data.AssetName.ToString());
				UObject* Asset = Data.GetAsset();
				UE_LOG(LogSpatialGDKEditor, Display, TEXT("Loaded Asset %s"), *GetNameSafe(Asset));
			}

			for (TPair<FName, FString> KV : Data.TagsAndValues.GetMap())
			{
				if (KV.Key != "FiBData")
				{
					UE_LOG(LogSpatialGDKEditor, Display, TEXT("-  %s = %s"), *KV.Key.ToString(), *KV.Value);
				}
			}
		}
	}


	TArray<ULevelStreaming*> LoadedLevels;

	return LoadedLevels;

	// Ensure all world composition tiles are also loaded
	if (World->WorldComposition != nullptr)
	{
		TArray<ULevelStreaming*> StreamingTiles = World->WorldComposition->TilesStreaming;

		UE_LOG(LogSpatialGDKEditor, Display, TEXT("Loading %d World Composition Tiles"), StreamingTiles.Num());
		for (ULevelStreaming* StreamingLevel : StreamingTiles)
		{
			TSharedPtr<FStreamableHandle> RequestHandle;

			TSoftObjectPtr<UWorld> WorldPtr = StreamingLevel->GetWorldAsset();
			if (WorldPtr.IsValid())
			{
				UE_LOG(LogSpatialGDKEditor, Display, TEXT("Level %s Already loaded, skipping"), *StreamingLevel->GetWorldAssetPackageName());
				continue;
			}
			else
			{
				LoadedLevels.Add(StreamingLevel);
				//LoadedLevels.Add(EditorLevelUtils::AddLevelToWorld(World, *StreamingLevel->GetWorldAssetPackageName(), ULevelStreamingKismet::StaticClass()));
				/*UObject* LoadedAsset = UAssetManager::GetStreamableManager().LoadSynchronous(StreamingLevel->GetWorldAsset(), false, &RequestHandle);
				UE_LOG(LogSpatialGDKEditor, Display, TEXT("Loading Level Tile %s [%s]"), *StreamingLevel->GetWorldAssetPackageName(), *GetNameSafe(LoadedAsset));*/
				//LoadedAssets.Add(StreamingLevel->GetWorldAsset().ToSoftObjectPath());
			}

			/*
			StreamingLevel->SetShouldBeVisible(true);
			StreamingLevel->SetShouldBeVisibleInEditor(false);
			StreamingLevel->bShouldBlockOnLoad = true;
			World->AddStreamingLevel(StreamingLevel);*/
		}
	}

	/*World->FlushLevelStreaming(EFlushLevelStreamingType::Full);*/
	return LoadedLevels;
}
