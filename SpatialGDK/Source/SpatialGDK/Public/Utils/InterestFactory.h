// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "Interop/SpatialClassInfoManager.h"
#include "Schema/Interest.h"

#include <WorkerSDK/improbable/c_worker.h>

class USpatialNetDriver;
class USpatialPackageMapClient;
class AActor;

DECLARE_LOG_CATEGORY_EXTERN(LogInterestFactory, Log, All);

namespace SpatialGDK
{

class SPATIALGDK_API InterestFactory
{
public:
	InterestFactory(AActor* InActor, const FClassInfo& InInfo, USpatialNetDriver* InNetDriver);

	Worker_ComponentData CreateInterestData();
	Worker_ComponentUpdate CreateInterestUpdate();

private:
	Interest CreateInterest();

private:
	// Checkout Constraint OR AlwaysInterested Constraint
	QueryConstraint CreateSystemDefinedConstraints();

	// System Defined Constraints
	QueryConstraint CreateCheckoutRadiusConstraint();
	QueryConstraint CreateAlwaysInterestedConstraint();

	void CreateActorQueries(TArray<Query> &InOutQueries);

	// Only checkout entities that are in loaded sublevels
	QueryConstraint CreateLevelConstraints();

	void AddObjectToConstraint(UObjectPropertyBase* Property, uint8* Data, QueryConstraint& OutConstraint);

	AActor* Actor;
	const FClassInfo& Info;
	USpatialNetDriver* NetDriver;
	USpatialPackageMapClient* PackageMap;
};

} // namespace SpatialGDK
