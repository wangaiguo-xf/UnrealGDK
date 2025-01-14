// Copyright (c) Improbable Worlds Ltd, All Rights Reserved	

#pragma once	

#include "Schema/Component.h"	
#include "SpatialConstants.h"	
#include "Utils/SchemaUtils.h"	

#include <WorkerSDK/improbable/c_schema.h>	
#include <WorkerSDK/improbable/c_worker.h>	

namespace SpatialGDK
{

struct ClientRPCEndpoint : Component
{
	static const Worker_ComponentId ComponentId = SpatialConstants::CLIENT_RPC_ENDPOINT_COMPONENT_ID;

	ClientRPCEndpoint() = default;

	Worker_ComponentData CreateRPCEndpointData()
	{
		Worker_ComponentData Data{};
		Data.component_id = ComponentId;
		Data.schema_type = Schema_CreateComponentData(ComponentId);
		Schema_Object* ComponentObject = Schema_GetComponentDataFields(Data.schema_type);
		Schema_AddBool(ComponentObject, SpatialConstants::UNREAL_RPC_ENDPOINT_READY_ID, bReady);

		return Data;
	}

	Worker_ComponentUpdate CreateRPCEndpointUpdate()
	{
		Worker_ComponentUpdate Update{};
		Update.component_id = ComponentId;
		Update.schema_type = Schema_CreateComponentUpdate(ComponentId);
		Schema_Object* UpdateObject = Schema_GetComponentUpdateFields(Update.schema_type);
		Schema_AddBool(UpdateObject, SpatialConstants::UNREAL_RPC_ENDPOINT_READY_ID, bReady);

		return Update;
	}

	bool bReady = false;
};

} // namespace SpatialGDK
