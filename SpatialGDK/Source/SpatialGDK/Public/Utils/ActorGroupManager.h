#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpatialConstants.h"

#include "ActorGroupManager.generated.h"

USTRUCT()
struct FWorkerType
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FName WorkerTypeName;

	FWorkerType()
	{
		WorkerTypeName = NAME_None;
	}
};

USTRUCT()
struct FActorGroupInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FName Name;

	UPROPERTY(EditAnywhere)
	FWorkerType OwningWorkerType;

	UPROPERTY(EditAnywhere)
	TSet<TSoftClassPtr<AActor>> ActorClasses;
	
	FActorGroupInfo()
	{
		Name = NAME_None;
		OwningWorkerType = FWorkerType();
	}
};

UCLASS(Config=SpatialGDKSettings)
class SPATIALGDK_API UActorGroupManager : public UObject
{
	GENERATED_BODY()

private:
	TMap<TSoftClassPtr<AActor>, FName> ClassPathToActorGroup;

	TMap<FName, FName> ActorGroupToWorkerType;

	FName DefaultWorkerType;

public:
	void Init();

	// Returns the first ActorGroup that contains this, or a parent of this class,
	// or the default actor group, if no mapping is found.
	FName GetActorGroupForClass(TSubclassOf<AActor> Class);

	// Returns the Server worker type that is authoritative over the ActorGroup
	// that contains this class (or parent class). Returns DefaultWorkerType
	// if no mapping is found.
	FName GetWorkerTypeForClass(TSubclassOf<AActor> Class);
};