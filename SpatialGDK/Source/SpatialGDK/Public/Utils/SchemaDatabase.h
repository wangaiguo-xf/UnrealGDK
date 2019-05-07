#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/World.h"
#include "SpatialConstants.h"

#include "SchemaDatabase.generated.h"

USTRUCT()
struct FSubobjectSchemaData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(VisibleAnywhere)
	FString ClassPath;

	UPROPERTY(VisibleAnywhere)
	FName Name;

	UPROPERTY(VisibleAnywhere)
	uint32 SchemaComponents[SCHEMA_Count] = {};
};

USTRUCT()
struct FSchemaData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(VisibleAnywhere)
	uint32 SchemaComponents[SCHEMA_Count] = {};

	UPROPERTY(VisibleAnywhere)
	TMap<uint32, FSubobjectSchemaData> SubobjectData;
};

UCLASS()
class SPATIALGDK_API USchemaDatabase : public UDataAsset
{
	GENERATED_BODY()

public:

	USchemaDatabase() : NextAvailableComponentId(SpatialConstants::STARTING_GENERATED_COMPONENT_ID) {}

	uint32 GetComponentIdFromLevelPath(const FString& LevelPath) const
	{
		FString CleanLevelPath = UWorld::RemovePIEPrefix(LevelPath);
		if (const uint32* ComponentId = LevelPathToComponentId.Find(CleanLevelPath))
		{
			return *ComponentId;
		}
		return SpatialConstants::INVALID_COMPONENT_ID;
	}

	/**
	 * Get the component ID of the marker component used to indicate that
	 * an entity represents an Unreal object of a particular type.
	 *
	 * If the type is not part of the schema, returns INVALID_COMPONENT_ID.
	 */
	uint32 GetComponentIdForClass(const UClass& Class) const
	{
		const FString ClassPath = Class.GetPathName();
		if (const FSchemaData* SchemaData = ClassPathToSchema.Find(ClassPath))
		{
			return SchemaData->SchemaComponents[SCHEMA_Data];
		}
		return SpatialConstants::INVALID_COMPONENT_ID;
	}

	UPROPERTY(VisibleAnywhere)
	TMap<FString, FSchemaData> ClassPathToSchema;

	UPROPERTY(VisibleAnywhere)
	TMap<FString, uint32> LevelPathToComponentId;

	UPROPERTY(VisibleAnywhere)
	TSet<uint32> LevelComponentIds;

	UPROPERTY(VisibleAnywhere)
	uint32 NextAvailableComponentId;
};

