// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "WorkerColorComponent.h"
#include "VisualizeWorkerColors.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealNetwork.h"

#pragma optimize("", off)

DEFINE_LOG_CATEGORY(LogSpatialOSToolkit);

// Sets default values for this component's properties
UWorkerColorComponent::UWorkerColorComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	bReplicates = true;
	CurrentMeshColor = FColor::Black;
	// ...
}

// Called when the game starts
void UWorkerColorComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UWorkerColorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWorkerColorComponent, CurrentMeshColor);
}

bool UWorkerColorComponent::Server_UpdateColorComponent_Validate()
{
	return true;
}

void UWorkerColorComponent::Server_UpdateColorComponent_Implementation()
{
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVisualizeWorkerColors::StaticClass(), FoundActors);

	if (FoundActors.Num())
	{
		if (AVisualizeWorkerColors* const WorkerColorObj = Cast<AVisualizeWorkerColors>(FoundActors[0]))
		{
			CurrentMeshColor = WorkerColorObj->GetObjectColorsInWorker();
		}
	}
	else
	{
		UE_LOG(LogSpatialOSToolkit, Warning, TEXT("A color component has been added to an actor without VisualizeWorkerColors actor being added to the level so ColorComponents will not be updated."));
	}
}

void UWorkerColorComponent::OnRep_CurrentMeshColor()
{
	OnCurrentMeshColorUpdated.Broadcast(CurrentMeshColor);
}
