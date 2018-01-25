// Copyright (c) Improbable Worlds Ltd, All Rights Reserved
// Note that this file has been generated automatically

#pragma once

#include <improbable/worker.h>
#include <improbable/view.h>
#include <unreal/generated/UnrealCharacter.h>
#include <unreal/core_types.h>
#include "SpatialHandlePropertyMap.h"
#include "SpatialTypeBinding.h"
#include "SpatialTypeBinding_Character.generated.h"

UCLASS()
class USpatialTypeBinding_Character : public USpatialTypeBinding
{
	GENERATED_BODY()
public:
	static const FRepHandlePropertyMap& GetHandlePropertyMap();
	void BindToView() override;
	void UnbindFromView() override;
	worker::ComponentId GetReplicatedGroupComponentId(EReplicatedPropertyGroup Group) const override;
	void SendComponentUpdates(const FPropertyChangeState& Changes, const worker::EntityId& EntityId) const override;
	worker::Entity CreateActorEntity(const FVector& Position, const FString& Metadata, const FPropertyChangeState& InitialChanges) const override;
private:
	worker::Dispatcher::CallbackKey SingleClientAddCallback;
	worker::Dispatcher::CallbackKey SingleClientUpdateCallback;
	worker::Dispatcher::CallbackKey MultiClientAddCallback;
	worker::Dispatcher::CallbackKey MultiClientUpdateCallback;
};
