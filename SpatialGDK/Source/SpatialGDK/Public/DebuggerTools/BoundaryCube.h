// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoundaryCube.generated.h"

class ABoundaryCube;
class UWorkerColorComponent;

DECLARE_DELEGATE_TwoParams(FBoundaryCubeOnAuthorityGained, int, ABoundaryCube*, InGridIndex, InBoundaryCube);


UCLASS(SpatialType)
class ABoundaryCube : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ABoundaryCube();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:

	void SetGridIndex(const int InGridIndex);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SetVisibility(bool bInIsVisible);

	UFUNCTION()
	bool GetIsVisible() { return bIsVisible; }

	UFUNCTION()
	void OnRep_IsVisible();

	UFUNCTION()
	void OnCurrentMeshColorUpdated(FColor InColor);

	virtual void OnAuthorityGained();

	static FBoundaryCubeOnAuthorityGained BoundaryCubeOnAuthorityGained;

protected:
	UFUNCTION(BlueprintImplementableEvent)
	void OnVisibilityUpdated(bool bInIsVisible);

private:
	UFUNCTION(CrossServer, Reliable, WithValidation)
	void CrossServer_SetVisibility(bool bInIsVisible);

private:
	UPROPERTY(Replicated)
	int GridIndex;

	UPROPERTY(ReplicatedUsing = OnRep_IsVisible)
	bool bIsVisible;

	UPROPERTY(EditAnywhere)
	USceneComponent* SceneComponent;

	UPROPERTY(EditAnywhere)
	UStaticMeshComponent* StaticMeshComponent;

	UPROPERTY()
	UWorkerColorComponent* WorkerColorComponent;

public:

	UFUNCTION()
	FColor GetCurrentMeshColor();
};
