// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "ActorInterestQueryComponent.generated.h"

namespace SpatialGDK
{
struct Query;
struct QueryConstraint;
}
class USchemaDatabase;

UCLASS(Abstract, BlueprintInternalUseOnly)
class UAbstractQueryConstraint : public UObject
{
	GENERATED_BODY()

public:
	virtual void CreateConstraint(const USchemaDatabase& SchemaDatabase, SpatialGDK::QueryConstraint& OutConstraint) const PURE_VIRTUAL(UAbstractQueryConstraint::CreateConstraint, );
};

/// Captures all entities within a fixed distance from the entity.
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class URelativeCylinderConstraint : public UAbstractQueryConstraint
{
	GENERATED_BODY()

public:
	virtual void CreateConstraint(const USchemaDatabase& SchemaDatabase, SpatialGDK::QueryConstraint& OutConstraint) const override;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Meta=(ClampMin=0))
	int32 Radius;
};

/// Captures all Actors of a given type.
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class UActorTypeConstraint : public UAbstractQueryConstraint
{
	GENERATED_BODY()

public:
	virtual void CreateConstraint(const USchemaDatabase& SchemaDatabase, SpatialGDK::QueryConstraint& OutConstraint) const override;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	TSubclassOf<AActor> ActorType;
};

/// Captures entities that pass all contained constraints.
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class UAndConstraint : public UAbstractQueryConstraint
{
	GENERATED_BODY()

public:
	virtual void CreateConstraint(const USchemaDatabase& SchemaDatabase, SpatialGDK::QueryConstraint& OutConstraint) const override;

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

	SpatialGDK::Query CreateQuery(const USchemaDatabase& SchemaDatabase) const;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Instanced)
	UAbstractQueryConstraint *Constraint;

	/// Rate of updates per second from SpatialOS, if any data needs updating.
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Meta = (ClampMin = 0.0))
	float UpdatesPerSecond;


};
