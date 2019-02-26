// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VisualizeWorkerColors.generated.h"

class ABoundaryCube;

//USTRUCT(BlueprintType)
//struct FDebugDataS
//{
//	GENERATED_USTRUCT_BODY()
//
//	UPROPERTY(BlueprintReadWrite)
//	FColor ObjectColor;
//
//	UPROPERTY(BlueprintReadWrite)
//	FVector Position;
//
//	UPROPERTY(BlueprintReadWrite)
//	TWeakObjectPtr<ABoundaryCube> DebugCube;
//
//	UPROPERTY()
//	int debugIndex;
//};

USTRUCT(BlueprintType)
struct FDebugBoundaryInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite)
	FVector Position;

	UPROPERTY(BlueprintReadWrite)
	TWeakObjectPtr<ABoundaryCube> DebugCube;
};

UCLASS(SpatialType)
class AVisualizeWorkerColors : public AActor
{
	GENERATED_BODY()

public:
	AVisualizeWorkerColors();

	virtual void BeginPlay();

	virtual void Tick(float DeltaTime);

	virtual void OnAuthorityGained();

	virtual void OnAuthorityLost();

	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable)
	void InitGrid2D();

	UFUNCTION(BlueprintCallable)
	const TArray<FDebugBoundaryInfo>& GetGrid2D() { return Grid2D; }

	UFUNCTION(CrossServer, Reliable, WithValidation, BlueprintCallable)
	void OnBoundaryCubeOnAuthorityGained(int InGridIndex, ABoundaryCube* InBoundaryCube);
	
	void CompareChuncks(const int CenterCell, TArray<uint32> CompareTo);

	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable)
	void SpawnBoundaryCubes();

	FColor GetObjectColorsInWorker() { return ObjectColorsInWorker; }

	UFUNCTION(BlueprintCallable)
	void BuildBoundaryWalls();

private:
	UFUNCTION()
	void UpdateCubeVisibility();

	void TurnOffAllCubeVisibility();

	bool IsOutterCube(const int CenterCell);

	UPROPERTY(Replicated)
	TArray<FDebugBoundaryInfo> Grid2D;

	FColor ObjectColorsInWorker;
};
