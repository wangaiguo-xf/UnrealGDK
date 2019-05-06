// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "Utils/InterestFactory.h"

#include "Engine/World.h"
#include "Engine/Classes/GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"

#include "Components/ActorInterestComponent.h"
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

	if (Actor->GetNetConnection() != nullptr)
	{
		return CreatePlayerOwnedActorInterest();
	}
	else
	{
		return CreateActorInterest();
	}
}

Interest InterestFactory::CreateActorInterest()
{
	Interest NewInterest;

	QueryConstraint SystemDefinedConstraints = CreateSystemDefinedConstraints();

	if (!SystemDefinedConstraints.IsValid())
	{
		return NewInterest;
	}

	Query NewQuery;
	NewQuery.Constraint = SystemDefinedConstraints;
	// TODO: Make result type handle components certain workers shouldn't see
	// e.g. Handover, OwnerOnly, etc.
	NewQuery.FullSnapshotResult = true;

	ComponentInterest NewComponentInterest;
	NewComponentInterest.Queries.Add(NewQuery);

	// Server Interest
	NewInterest.ComponentInterestMap.Add(SpatialConstants::POSITION_COMPONENT_ID, NewComponentInterest);

	return NewInterest;
}

Interest InterestFactory::CreatePlayerOwnedActorInterest()
{
	QueryConstraint SystemDefinedConstraints = CreateSystemDefinedConstraints();

	// Servers only need the defined constraints
	Query ServerQuery;
	ServerQuery.Constraint = SystemDefinedConstraints;
	ServerQuery.FullSnapshotResult = true;

	ComponentInterest ServerComponentInterest;
	ServerComponentInterest.Queries.Add(ServerQuery);

	// Clients should only check out entities that are in loaded sublevels
	QueryConstraint LevelConstraints = CreateLevelConstraints();

	QueryConstraint ClientConstraint;

	if (SystemDefinedConstraints.IsValid())
	{
		ClientConstraint.AndConstraint.Add(SystemDefinedConstraints);
	}

	if (LevelConstraints.IsValid())
	{
		ClientConstraint.AndConstraint.Add(LevelConstraints);
	}

	Query ClientQuery;
	ClientQuery.Constraint = ClientConstraint;
	ClientQuery.FullSnapshotResult = true;

	ComponentInterest ClientComponentInterest;
	ClientComponentInterest.Queries.Add(ClientQuery);

	Interest NewInterest;
	// Server Interest
	if (SystemDefinedConstraints.IsValid())
	{
		NewInterest.ComponentInterestMap.Add(SpatialConstants::POSITION_COMPONENT_ID, ServerComponentInterest);
	}
	// Client Interest
	if (ClientConstraint.IsValid())
	{
		NewInterest.ComponentInterestMap.Add(SpatialConstants::CLIENT_RPC_ENDPOINT_COMPONENT_ID, ClientComponentInterest);
	}

	return NewInterest;
}

QueryConstraint InterestFactory::CreateSystemDefinedConstraints()
{
	QueryConstraint ActorInterestConstraint = CreateActorInterestConstraint();
	QueryConstraint AlwaysInterestedConstraint = CreateAlwaysInterestedConstraint();

	QueryConstraint SystemDefinedConstraints;

	if (ActorInterestConstraint.IsValid())
	{
		SystemDefinedConstraints.OrConstraint.Add(ActorInterestConstraint);
	}

	if (AlwaysInterestedConstraint.IsValid())
	{
		SystemDefinedConstraints.OrConstraint.Add(AlwaysInterestedConstraint);
	}

	return SystemDefinedConstraints;
}

QueryConstraint InterestFactory::CreateActorInterestConstraint()
{
	QueryConstraint ActorInterestConstraint;

	TArray<UActorInterestComponent*> InterestComponents;
	Actor->GetComponents<UActorInterestComponent>(InterestComponents);
	float DefaultCheckoutRadius = 0.0f;
	for (const UActorInterestComponent* InterestComponent : InterestComponents)
	{
		// TODO - timgibson - move logic for creating actor interest constraints to the component?

		DefaultCheckoutRadius = FMath::Max(DefaultCheckoutRadius, static_cast<float>(InterestComponent->DefaultCheckoutRadius) / 100.0f);
		check(DefaultCheckoutRadius >= 0);

		for (const FActorInterestRadius& ActorInterest: InterestComponent->Interests)
		{
			QueryConstraint ParticularActorTypeConstraint;

			float RadiusMeters = static_cast<float>(ActorInterest.InterestRadius) / 100.0f;
			check(RadiusMeters >= 0);
			QueryConstraint RadiusConststraint;
			RadiusConststraint.RelativeCylinderConstraint = RelativeCylinderConstraint{ RadiusMeters };
			ParticularActorTypeConstraint.AndConstraint.Add(RadiusConststraint);

			// TODO - timgibson - will this work in all cases that we care about?
			// See comments here about using the asset registry:
			// http://kantandev.com/articles/finding-all-classes-blueprints-with-a-given-base
			QueryConstraint ComponentConstraints;
			for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
			{
				UClass* Class = *ClassIt;
				if (Class->IsChildOf(ActorInterest.ActorType))
				{
					const uint32 ComponentId = NetDriver->ClassInfoManager->SchemaDatabase->GetComponentIdForClass(Class);
					if (ComponentId != SpatialConstants::INVALID_COMPONENT_ID)
					{
						QueryConstraint ComponentTypeConstraint;
						ComponentTypeConstraint.ComponentConstraint = ComponentId;
						ComponentConstraints.OrConstraint.Add(ComponentTypeConstraint);
					}
				}
			}
			ParticularActorTypeConstraint.AndConstraint.Add(ComponentConstraints);

			ActorInterestConstraint.OrConstraint.Add(ParticularActorTypeConstraint);
		}
	}
	QueryConstraint DefaultRadiusConstraint;
	DefaultRadiusConstraint.RelativeCylinderConstraint = RelativeCylinderConstraint{ DefaultCheckoutRadius };
	ActorInterestConstraint.OrConstraint.Add(DefaultRadiusConstraint);

	return ActorInterestConstraint;

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
