// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "ActorInterestQueryComponent.h"

#include "Schema/Interest.h"
#include "Utils/SchemaDatabase.h"

void URelativeCylinderConstraint::CreateConstraint(const USchemaDatabase& SchemaDatabase, SpatialGDK::QueryConstraint& OutConstraint) const
{
	double RadiusMeters = static_cast<double>(Radius) / 100.0;
	OutConstraint.RelativeCylinderConstraint = SpatialGDK::RelativeCylinderConstraint{ RadiusMeters };
}

void UActorTypeConstraint::CreateConstraint(const USchemaDatabase& SchemaDatabase, SpatialGDK::QueryConstraint& OutConstraint) const
{
	// Iterate all classes to find those derived from the AActor class that we
	// care about, and add those to an OrConstraint.

	// TODO - timgibson - will this work in all cases that we care about?
	// See comments here about using the asset registry:
	// http://kantandev.com/articles/finding-all-classes-blueprints-with-a-given-base
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		check(Class);
		if (Class->IsChildOf(ActorType))
		{
			const uint32 ComponentId = SchemaDatabase.GetComponentIdForClass(*Class);
			if (ComponentId != SpatialConstants::INVALID_COMPONENT_ID)
			{
				SpatialGDK::QueryConstraint ComponentTypeConstraint;
				ComponentTypeConstraint.ComponentConstraint = ComponentId;
				OutConstraint.OrConstraint.Add(ComponentTypeConstraint);
			}
		}
	}
}

void UAndConstraint::CreateConstraint(const USchemaDatabase& SchemaDatabase, SpatialGDK::QueryConstraint& OutConstraint) const
{
	for (const UAbstractQueryConstraint* ConstraintData : Constraints)
	{
		SpatialGDK::QueryConstraint NewConstraint;
		ConstraintData->CreateConstraint(SchemaDatabase, NewConstraint);
		OutConstraint.AndConstraint.Add(NewConstraint);
	}
}

UActorInterestQueryComponent::UActorInterestQueryComponent()
	: UpdatesPerSecond(0)
{
	PrimaryComponentTick.bCanEverTick = false;
}

SpatialGDK::Query UActorInterestQueryComponent::CreateQuery(const USchemaDatabase& SchemaDatabase) const
{
	SpatialGDK::Query InterestQuery;
	InterestQuery.Frequency = UpdatesPerSecond;
	Constraint->CreateConstraint(SchemaDatabase, InterestQuery.Constraint);

	// TODO: Make result type handle components certain workers shouldn't see
	// e.g. Handover, OwnerOnly, etc.
	InterestQuery.FullSnapshotResult = true;

	return InterestQuery;
}
