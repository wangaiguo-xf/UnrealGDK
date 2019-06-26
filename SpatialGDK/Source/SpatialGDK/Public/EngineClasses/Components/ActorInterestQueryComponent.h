// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EngineClasses/SpatialInterestConstraints.h"

#include "ActorInterestQueryComponent.generated.h"

namespace SpatialGDK
{
struct Query;
}
class USchemaDatabase;

UCLASS(ClassGroup=(SpatialGDK), NotSpatialType, Meta=(BlueprintSpawnableComponent))
class SPATIALGDK_API UActorInterestQueryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UActorInterestQueryComponent();

	SpatialGDK::Query CreateQuery(const USchemaDatabase& SchemaDatabase) const;

	UPROPERTY(BlueprintReadonly, EditDefaultsOnly, Instanced)
	UAbstractQueryConstraint* Constraint;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Meta=(ClampMin=0.0))
	float Frequency;
};
