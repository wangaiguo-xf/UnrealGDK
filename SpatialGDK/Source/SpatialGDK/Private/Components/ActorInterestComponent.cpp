// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "ActorInterestComponent.h"


// Sets default values for this component's properties
UActorInterestComponent::UActorInterestComponent()
	: DefaultCheckoutRadius(0.0f)
{
	check(PrimaryComponentTick.bCanEverTick == false);
}


// Called when the game starts
void UActorInterestComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


