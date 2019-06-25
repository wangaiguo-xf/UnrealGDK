// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include <WorkerSDK/improbable/c_worker.h>
#include "Interop/SpatialDispatcher.h"
#include "ExternalWorkerInteropTemplate.generated.h"

UCLASS(abstract)
class SPATIALGDK_API UExternalWorkerInteropTemplate : public UActorComponent
{
	GENERATED_BODY()

public:
	UExternalWorkerInteropTemplate() = default;
	~UExternalWorkerInteropTemplate() = default;

	/**
	  * For each SpatialOS component encapsulated by this Actor component, register callbacks for any ops received.
	  */
	void Init(USpatialDispatcher* Dispatcher) {
		const TMap<Worker_ComponentId, WorkerRequirementSet>& ExternalComponentDetails = GetExternalComponentDetails();
		for (const Worker_ComponentId* ComponentId : ExternalComponentDetails.GetKeys())
		{
			Dispatcher->OnAddComponent(ComponentId, [this](const Worker_AddComponentOp& Op) { this->OnAddComponent(ComponentId, Op); });
			Dispatcher->OnRemoveComponent(ComponentId, [this](const Worker_RemoveComponentOp& Op) { this->OnRemoveComponent(ComponentId, Op); });
			Dispatcher->OnAuthorityChange(ComponentId, [this](const Worker_AuthorityChangeOp& Op) { this->OnAuthorityChange(ComponentId, Op); });
			Dispatcher->OnComponentUpdate(ComponentId, [this](const Worker_ComponentUpdateOp& Op) { this->OnComponentUpdate(ComponentId, Op); });
			Dispatcher->OnCommandRequest(ComponentId, [this](const Worker_CommandRequestOp& Op) { this->OnCommandRequest(ComponentId, Op); });
			Dispatcher->OnCommandResponse(ComponentId, [this](const Worker_CommandResponseOp& Op) { this->OnCommandResponse(ComponentId, Op); });
		}
	}

	/**
	  * These methods are called by the SpatialReceiver when a network operation for one of the component IDs in the map
	  * returned by GetExternalComponentDetails() is received for this actor.
	  */
	virtual void OnAddComponent(Worker_ComponentId, const Worker_AddComponentOp&) {};
	virtual void OnRemoveComponent(Worker_ComponentId, const Worker_RemoveComponentOp&) {};
	virtual void OnAuthorityChange(Worker_ComponentId, const Worker_AuthorityChangeOp&) {};
	virtual void OnComponentUpdate(Worker_ComponentId, const Worker_ComponentUpdateOp&) {};
	virtual void OnCommandRequest(Worker_ComponentId, const Worker_CommandRequestOp&) {};
	virtual void OnCommandResponse(Worker_ComponentId, const Worker_CommandResponseOp&) {};

	/**
	  * Return the mapping from external SpatialOS components and respective write ACL requirement sets to be managed by this component.
	  * @return Map<Worker_ComponentId, WorkerRequirementSet> mapping from SpatialOS component to write ACL requirement sets.
	  */
	virtual const TMap<Worker_ComponentId, WorkerRequirementSet>& GetExternalComponentDetails() const PURE_VIRTUAL(UExternalWorkerInteropTemplate::GetExternalComponentDetails, return new TMap<Worker_ComponentId, WorkerRequirementSet>(););
};
