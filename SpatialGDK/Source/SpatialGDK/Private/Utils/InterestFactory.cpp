// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "Utils/InterestFactory.h"

#include "Engine/World.h"
#include "Engine/Classes/GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"

#include "Components/ActorInterestQueryComponent.h"
#include "EngineClasses/SpatialNetConnection.h"
#include "EngineClasses/SpatialNetDriver.h"
#include "EngineClasses/SpatialPackageMapClient.h"
#include "SpatialGDKSettings.h"
#include "SpatialConstants.h"
#include "UObjectIterator.h"

DEFINE_LOG_CATEGORY(LogInterestFactory);

namespace SpatialGDK
{

InterestFactory::InterestFactory(AActor* InActor, const FClassInfo& InInfo, USpatialNetDriver* InNetDriver)
	: Actor(InActor)
	, Info(InInfo)
	, NetDriver(InNetDriver)
	, PackageMap(InNetDriver->PackageMap)
{}

Worker_ComponentData InterestFactory::CreateInterestData()
{
	return CreateInterest().CreateInterestData();
}

Worker_ComponentUpdate InterestFactory::CreateInterestUpdate()
{
	return CreateInterest().CreateInterestUpdate();
}

Interest InterestFactory::CreateInterest()
{
	if (!GetDefault<USpatialGDKSettings>()->bUsingQBI)
	{
		return Interest{};
	}

	Interest NewInterest;

	// System constraints are relevant for client and server workers.
	QueryConstraint SystemDefinedConstraints = CreateSystemDefinedConstraints();

	// TODO - timgibson - it might make more sense to attach these to the component
	// representing this.Actor's type instead of POSTITION and CLIENT_RPC
	TArray<Query> ActorQueries;
	CreateActorQueries(ActorQueries);

	// Server Interest
	{
		ComponentInterest ServerQueries;

		// TODO: Make result type handle components certain workers shouldn't see
		// e.g. Handover, OwnerOnly, etc.
		Query ServerQuery;
		ServerQuery.Constraint = SystemDefinedConstraints;
		ServerQuery.FullSnapshotResult = true;

		if (ServerQuery.Constraint.IsValid())
		{
			ServerQueries.Queries.Add(ServerQuery);
		}
		ServerQueries.Queries.Append(ActorQueries);

		if (ServerQueries.Queries.Num() > 0)
		{
			NewInterest.ComponentInterestMap.Add(SpatialConstants::POSITION_COMPONENT_ID, ServerQueries);
		}
	}

	// Client Interest
	if (Actor->GetNetConnection() != nullptr)
	{
		QueryConstraint ClientConstraint;

		if (SystemDefinedConstraints.IsValid())
		{
			ClientConstraint.AndConstraint.Add(SystemDefinedConstraints);
		}

		QueryConstraint LevelConstraints = CreateLevelConstraints();
		if (LevelConstraints.IsValid())
		{
			ClientConstraint.AndConstraint.Add(LevelConstraints);
		}

		// TODO: Make result type handle components certain workers shouldn't see
		// e.g. Handover, OwnerOnly, etc.

		Query ClientQuery;
		ClientQuery.Constraint = ClientConstraint;
		ClientQuery.FullSnapshotResult = true;

		ComponentInterest ClientQueries;
		if (ClientQuery.Constraint.IsValid())
		{
			ClientQueries.Queries.Add(ClientQuery);
		}
		ClientQueries.Queries.Append(ActorQueries);

		if (ClientQueries.Queries.Num() > 0)
		{
			NewInterest.ComponentInterestMap.Add(SpatialConstants::CLIENT_RPC_ENDPOINT_COMPONENT_ID, ClientQueries);
		}
	}

	return NewInterest;
}

QueryConstraint InterestFactory::CreateSystemDefinedConstraints()
{
	QueryConstraint CheckoutRadiusConstraint = CreateCheckoutRadiusConstraint();
	QueryConstraint AlwaysInterestedConstraint = CreateAlwaysInterestedConstraint();

	QueryConstraint SystemDefinedConstraints;

	if (CheckoutRadiusConstraint.IsValid())
	{
		SystemDefinedConstraints.OrConstraint.Add(CheckoutRadiusConstraint);
	}

	if (AlwaysInterestedConstraint.IsValid())
	{
		SystemDefinedConstraints.OrConstraint.Add(AlwaysInterestedConstraint);
	}

	return SystemDefinedConstraints;
}

void InterestFactory::CreateActorQueries(TArray<Query> &InOutQueries)
{
	check(Actor);
	check(NetDriver && NetDriver->ClassInfoManager && NetDriver->ClassInfoManager->SchemaDatabase);

	TArray<UActorInterestQueryComponent *> InterestComponents;
	Actor->GetComponents<UActorInterestQueryComponent>(InterestComponents);
	for (const UActorInterestQueryComponent* InterestComponent : InterestComponents)
	{
		Query ActorInterestQuery = InterestComponent->CreateQuery(*NetDriver->ClassInfoManager->SchemaDatabase);
		InOutQueries.Add(ActorInterestQuery);
	}
}

QueryConstraint InterestFactory::CreateCheckoutRadiusConstraint()
{
	QueryConstraint CheckoutRadiusConstraint;

	// TODO - timgibson - Replace actor-based default checkout radius with a setting?
	float CheckoutRadius = Actor->CheckoutRadius / 100.0f; // Convert to meters
	CheckoutRadiusConstraint.RelativeCylinderConstraint = RelativeCylinderConstraint{ CheckoutRadius };

	return CheckoutRadiusConstraint;
}

QueryConstraint InterestFactory::CreateAlwaysInterestedConstraint()
{
	QueryConstraint AlwaysInterestedConstraint;

	for (const FInterestPropertyInfo& PropertyInfo : Info.InterestProperties)
	{
		uint8* Data = (uint8*)Actor + PropertyInfo.Offset;
		if (UObjectPropertyBase* ObjectProperty = Cast<UObjectPropertyBase>(PropertyInfo.Property))
		{
			AddObjectToConstraint(ObjectProperty, Data, AlwaysInterestedConstraint);
		}
		else if (UArrayProperty* ArrayProperty = Cast<UArrayProperty>(PropertyInfo.Property))
		{
			FScriptArrayHelper ArrayHelper(ArrayProperty, Data);
			for (int i = 0; i < ArrayHelper.Num(); i++)
			{
				AddObjectToConstraint(Cast<UObjectPropertyBase>(ArrayProperty->Inner), ArrayHelper.GetRawPtr(i), AlwaysInterestedConstraint);
			}
		}
		else
		{
			checkNoEntry();
		}
	}

	return AlwaysInterestedConstraint;
}

void InterestFactory::AddObjectToConstraint(UObjectPropertyBase* Property, uint8* Data, QueryConstraint& OutConstraint)
{
	UObject* ObjectOfInterest = Property->GetObjectPropertyValue(Data);

	if (ObjectOfInterest == nullptr)
	{
		return;
	}

	FUnrealObjectRef UnrealObjectRef = PackageMap->GetUnrealObjectRefFromObject(ObjectOfInterest);

	if (!UnrealObjectRef.IsValid())
	{
		return;
	}

	QueryConstraint EntityIdConstraint;
	EntityIdConstraint.EntityIdConstraint = UnrealObjectRef.Entity;
	OutConstraint.OrConstraint.Add(EntityIdConstraint);
}

QueryConstraint InterestFactory::CreateLevelConstraints()
{
	QueryConstraint LevelConstraint;

	QueryConstraint DefaultConstraint;
	DefaultConstraint.ComponentConstraint = SpatialConstants::NOT_STREAMED_COMPONENT_ID;
	LevelConstraint.OrConstraint.Add(DefaultConstraint);

	UNetConnection* Connection = Actor->GetNetConnection();
	check(Connection);
	APlayerController* PlayerController = Connection->GetPlayerController(nullptr);
	check(PlayerController);

	const TSet<FName>& LoadedLevels = PlayerController->NetConnection->ClientVisibleLevelNames;

	// Create component constraints for every loaded sublevel
	for (const auto& LevelPath : LoadedLevels)
	{
		const uint32 ComponentId = NetDriver->ClassInfoManager->SchemaDatabase->GetComponentIdFromLevelPath(LevelPath.ToString());
		if (ComponentId != SpatialConstants::INVALID_COMPONENT_ID)
		{
			QueryConstraint SpecificLevelConstraint;
			SpecificLevelConstraint.ComponentConstraint = ComponentId;
			LevelConstraint.OrConstraint.Add(SpecificLevelConstraint);
		}
		else
		{
			UE_LOG(LogInterestFactory, Error, TEXT("Error creating query constraints for Actor %s. "
				"Could not find Streaming Level Component for Level %s. Have you generated schema?"), *Actor->GetName(), *LevelPath.ToString());
		}
	}

	return LevelConstraint;
}

} // namespace SpatialGDK
