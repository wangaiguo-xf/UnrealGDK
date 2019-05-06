// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActorInterestQueryComponent.generated.h"

UCLASS(Abstract, BlueprintInternalUseOnly)
class UAbstractQueryConstraint : public UObject
{
	GENERATED_BODY()
};

/// Captures all entities within a fixed distance from the entity.
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class URelativeCylinderConstraint : public UAbstractQueryConstraint
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Meta=(ClampMin=0))
	int32 Radius;
};

/// Captures all Actors of a given type.
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class UActorTypeConstraint : public UAbstractQueryConstraint
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	TSubclassOf<AActor> ActorType;
};

/// Captures entities that pass all contained constraints.
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class UAndConstraint : public UAbstractQueryConstraint
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Instanced)
	TArray<UAbstractQueryConstraint *> Constraints;
};

/// Defines what information an actor needs from SpatialOS to perform its work.
UCLASS(ClassGroup = (SpatialOS), Meta = (BlueprintSpawnableComponent))
class SPATIALGDK_API UActorInterestQueryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UActorInterestQueryComponent();

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Instanced)
	UAbstractQueryConstraint *Constraint;

	/// Rate of updates per second from SpatialOS, if any data needs updating.
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Meta = (ClampMin = 0))
	int32 UpdatesPerSecond;


};
