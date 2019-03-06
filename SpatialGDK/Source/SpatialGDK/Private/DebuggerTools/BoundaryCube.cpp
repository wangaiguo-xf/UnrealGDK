// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "BoundaryCube.h"
#include "PackageName.h"
#include "ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "WorkerColorComponent.h"
#include "UnrealNetwork.h"

FBoundaryCubeOnAuthorityGained ABoundaryCube::BoundaryCubeOnAuthorityGained;
// Sets default values
ABoundaryCube::ABoundaryCube()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BoxMesh(TEXT("StaticMesh'/Game/Geometry/Meshes/1M_Cube.1M_Cube'"));
	static ConstructorHelpers::FObjectFinder<UMaterial> BaseMaterial(TEXT("Material'/Game/Vehicle/Meshes/BaseMaterial.BaseMaterial'"));



	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
	StaticMeshComponent->SetStaticMesh(BoxMesh.Object);
	StaticMeshComponent->SetMaterial(0, UMaterialInstanceDynamic::Create(BaseMaterial.Object, StaticMeshComponent));
	StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetRootComponent(StaticMeshComponent);

	WorkerColorComponent = CreateDefaultSubobject<UWorkerColorComponent>(TEXT("WorkerColorComponent"));
	WorkerColorComponent->OnCurrentMeshColorUpdated.AddDynamic(this, &ABoundaryCube::OnCurrentMeshColorUpdated);

	GridIndex	= -1;

	bReplicates = true;
	bIsVisible  = true;
}

// Called when the game starts or when spawned
void ABoundaryCube::BeginPlay()
{
	Super::BeginPlay();
}

void ABoundaryCube::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABoundaryCube, bIsVisible);
	DOREPLIFETIME(ABoundaryCube, GridIndex);
}

void ABoundaryCube::OnCurrentMeshColorUpdated(FColor InColor)
{
	UMaterialInstanceDynamic* MaterialInstance = StaticMeshComponent->CreateDynamicMaterialInstance(0, StaticMeshComponent->GetMaterial(0));
	MaterialInstance->SetVectorParameterValue(FName("DiffuseColor"), FLinearColor(InColor));
	StaticMeshComponent->SetMaterial(0, MaterialInstance);
}

void ABoundaryCube::OnAuthorityGained()
{
	WorkerColorComponent->Server_UpdateColorComponent();
	if (BoundaryCubeOnAuthorityGained.IsBound() && GridIndex != -1)
	{
		BoundaryCubeOnAuthorityGained.Execute(GridIndex, this);
	}
}
FColor ABoundaryCube::GetCurrentMeshColor()
{
	return WorkerColorComponent->GetCurrentMeshColor();
}

void ABoundaryCube::SetGridIndex(const int InGridIndex)
{
	GridIndex = InGridIndex;
}

void ABoundaryCube::OnRep_IsVisible()
{
	StaticMeshComponent->SetVisibility(bIsVisible);
}

bool ABoundaryCube::Server_SetVisibility_Validate(bool bInIsVisible)
{
	return true;
}

void ABoundaryCube::Server_SetVisibility_Implementation(bool bInIsVisible)
{
	CrossServer_SetVisibility(bInIsVisible);
}

bool ABoundaryCube::CrossServer_SetVisibility_Validate(bool bInIsVisible)
{
	return true;
}

void ABoundaryCube::CrossServer_SetVisibility_Implementation(bool bInIsVisible)
{
	bIsVisible = bInIsVisible;
}

bool ABoundaryCube::DestroyCube_Validate()
{
	return true;
}

void ABoundaryCube::DestroyCube_Implementation()
{
	CrossServer_DestroyCube();
}


bool ABoundaryCube::CrossServer_DestroyCube_Validate()
{
	return true;
}

void ABoundaryCube::CrossServer_DestroyCube_Implementation()
{
	Destroy();
}
