// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WorkerColorComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSpatialOSToolkit, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCurrentMeshColorUpdated, FColor, InColor);

UCLASS(SpatialType, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class  UWorkerColorComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UWorkerColorComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable)
	void Server_UpdateColorComponent();
		
	UFUNCTION()
	void OnRep_CurrentMeshColor();

	UFUNCTION()
	FColor GetCurrentMeshColor() { return CurrentMeshColor; }

	UPROPERTY(BlueprintAssignable)
	FOnCurrentMeshColorUpdated OnCurrentMeshColorUpdated;

private:
	UPROPERTY(ReplicatedUsing = OnRep_CurrentMeshColor)
	FColor CurrentMeshColor;
};
