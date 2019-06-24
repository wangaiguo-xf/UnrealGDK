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
	FString WorkerTypeName;

	FWorkerType()
	{
		WorkerTypeName = "";
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

USTRUCT()
struct FWorkerAssociation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TMap<FName, FString> ActorGroupToWorker;

	FWorkerAssociation()
	{
		ActorGroupToWorker = {};
	}

	FWorkerAssociation(TMap<FName, FString> Values)
	{
		ActorGroupToWorker = TMap<FName, FString>(Values);
	}
};

UCLASS(Config=SpatialGDKSettings)
class SPATIALGDK_API UActorGroupManager : public UObject
{
	GENERATED_BODY()

private:
	TMap<TSoftClassPtr<AActor>, FName> ClassPathToActorGroup;

	TMap<FName, FString> ActorGroupToWorkerType;

	UActorGroupManager();

	void InitFromSettings();

public:

	static UActorGroupManager* GetInstance();

	FName GetActorGroupForClass(UClass* Class);

	FString GetWorkerTypeForClass(UClass* Class);
};
