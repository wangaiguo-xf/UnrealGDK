// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "ActorInterestQueryComponent.h"


UActorInterestQueryComponent::UActorInterestQueryComponent()
	: UpdatesPerSecond(0)
{
	PrimaryComponentTick.bCanEverTick = false;
}

