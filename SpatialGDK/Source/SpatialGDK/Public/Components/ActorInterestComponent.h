// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActorInterestComponent.generated.h"

USTRUCT(BlueprintType)
struct FActorInterestRadius
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	TSubclassOf<AActor> ActorType;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Meta=(ClampMin=0))
	int32 InterestRadius;

	//UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	//float UpdateFrequency;
};

UCLASS(ClassGroup = (SpatialOS), meta = (BlueprintSpawnableComponent))
class SPATIALGDK_API UActorInterestComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UActorInterestComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	

	/* This Actor will check out all other Actors within this radius. Defined in centimeters.
	 * @see https://docs.improbable.io/reference/13.6/shared/worker-configuration/bridge-config#entity-interest
	 */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Meta=(ClampMin=0))
	int32 DefaultCheckoutRadius;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Meta=(TitleProperty=ActorType))
	TArray<FActorInterestRadius> Interests;
	
};
