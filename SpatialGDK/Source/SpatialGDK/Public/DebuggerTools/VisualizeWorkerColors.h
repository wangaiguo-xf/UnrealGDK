// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VisualizeWorkerColors.generated.h"


class ABoundaryCube;

USTRUCT(BlueprintType)
struct FDebugBoundaryInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite)
	FVector SpawnPosition;

	UPROPERTY(BlueprintReadWrite)
	TWeakObjectPtr<ABoundaryCube> DebugCube;
};

UCLASS(SpatialType)
class SPATIALGDK_API AVisualizeWorkerColors : public AActor
{
	GENERATED_BODY()

public:
	AVisualizeWorkerColors();

	virtual void BeginPlay();

	virtual void Tick(float DeltaTime);

	virtual void OnAuthorityGained();

	virtual void OnAuthorityLost();

	UFUNCTION(Server, Reliable, WithValidation)
	void InitGrid2D();

	UFUNCTION(BlueprintCallable)
	const TArray<FDebugBoundaryInfo>& GetGrid2D() { return Grid2D; }

	UFUNCTION(CrossServer, Reliable, WithValidation)
	void OnBoundaryCubeOnAuthorityGained(int InGridIndex, ABoundaryCube* InBoundaryCube);

	FColor GetObjectColorsInWorker() { return ObjectColorsInWorker; }

private:
	UFUNCTION(Server, Reliable, WithValidation)
	void SpawnBoundaryCubes();

	void UpdateCubeVisibility();

	void TurnOffAllCubeVisibility();

	void CreateBoundaryWalls();

	void DeleteBoundaryWalls();

	void CompareChuncks(const int CenterCell, TArray<uint32> CompareTo);

	bool IsOutterCube(const int CenterCell);

	bool AreColorsMatching(const uint32 InLhs, const uint32 InRhs);

	UPROPERTY(Handover)
	TArray<FDebugBoundaryInfo> Grid2D;

	UPROPERTY(Handover)
	TArray<ABoundaryCube*> CachedBoundaryWalls;

	/** Keeps track of the last index iterating through Grid2D to spawn BoundaryCubes */
	UPROPERTY(Handover)
	int LastSpawnedIndex;

	FColor ObjectColorsInWorker;


};
