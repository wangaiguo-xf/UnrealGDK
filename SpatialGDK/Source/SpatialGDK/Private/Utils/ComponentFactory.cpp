// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "Utils/ComponentFactory.h"

#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/World.h"
#include "UObject/TextProperty.h"

#include "EngineClasses/SpatialActorChannel.h"
#include "EngineClasses/SpatialFastArrayNetSerialize.h"
#include "EngineClasses/SpatialNetBitWriter.h"
#include "EngineClasses/SpatialNetDriver.h"
#include "EngineClasses/SpatialPackageMapClient.h"
#include "Schema/Interest.h"
#include "SpatialConstants.h"
#include "Utils/RepLayoutUtils.h"
#include "Utils/InterestFactory.h"

namespace SpatialGDK
{

ComponentFactory::ComponentFactory(FUnresolvedObjectsMap& RepUnresolvedObjectsMap, FUnresolvedObjectsMap& HandoverUnresolvedObjectsMap, bool bInterestDirty, USpatialNetDriver* InNetDriver)
	: NetDriver(InNetDriver)
	, PackageMap(InNetDriver->PackageMap)
	, ClassInfoManager(InNetDriver->ClassInfoManager)
	, PendingRepUnresolvedObjectsMap(RepUnresolvedObjectsMap)
	, PendingHandoverUnresolvedObjectsMap(HandoverUnresolvedObjectsMap)
	, bInterestHasChanged(bInterestDirty)
{ }

bool ComponentFactory::FillSchemaObject(Schema_Object* ComponentObject, UObject* Object, const FRepChangeState& Changes, ESchemaComponentType PropertyGroup, bool bIsInitialData, TArray<Schema_FieldId>* ClearedIds /*= nullptr*/)
{
	bool bWroteSomething = false;

	// Populate the replicated data component updates from the replicated property changelist.
	if (Changes.RepChanged.Num() > 0)
	{
		FChangelistIterator ChangelistIterator(Changes.RepChanged, 0);
		FRepHandleIterator HandleIterator(ChangelistIterator, Changes.RepLayout.Cmds, Changes.RepLayout.BaseHandleToCmdIndex, 0, 1, 0, Changes.RepLayout.Cmds.Num() - 1);
		while (HandleIterator.NextHandle())
		{
			const FRepLayoutCmd& Cmd = Changes.RepLayout.Cmds[HandleIterator.CmdIndex];
			const FRepParentCmd& Parent = Changes.RepLayout.Parents[Cmd.ParentIndex];

			if (GetGroupFromCondition(Parent.Condition) == PropertyGroup)
			{
				const uint8* Data = (uint8*)Object + Cmd.Offset;
				TSet<TWeakObjectPtr<const UObject>> UnresolvedObjects;

				bool bProcessedFastArrayProperty = false;

				if (Cmd.Type == ERepLayoutCmdType::DynamicArray)
				{
					UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Cmd.Property);

					// Check if this is a FastArraySerializer array and if so, call our custom delta serialization
					if (UScriptStruct* NetDeltaStruct = GetFastArraySerializerProperty(ArrayProperty))
					{
						FSpatialNetBitWriter ValueDataWriter(PackageMap, UnresolvedObjects);

						FSpatialNetDeltaSerializeInfo::DeltaSerializeWrite(NetDriver, ValueDataWriter, Object, Parent.ArrayIndex, Parent.Property, NetDeltaStruct);

						AddBytesToSchema(ComponentObject, HandleIterator.Handle, ValueDataWriter);

						bProcessedFastArrayProperty = true;
					}
				}

				if (!bProcessedFastArrayProperty)
				{
					AddProperty(ComponentObject, HandleIterator.Handle, Cmd.Property, Data, UnresolvedObjects, ClearedIds);
				}

				if (UnresolvedObjects.Num() == 0)
				{
					bWroteSomething = true;
				}
				else
				{
					if (!bIsInitialData)
					{
						// Don't send updates for fields with unresolved objects, unless it's the initial data,
						// in which case all fields should be populated.
						Schema_ClearField(ComponentObject, HandleIterator.Handle);
					}

					PendingRepUnresolvedObjectsMap.Add(HandleIterator.Handle, UnresolvedObjects);
				}
			}

			if (Cmd.Type == ERepLayoutCmdType::DynamicArray)
			{
				if (!HandleIterator.JumpOverArray())
				{
					break;
				}
			}
		}
	}

	return bWroteSomething;
}

bool ComponentFactory::FillHandoverSchemaObject(Schema_Object* ComponentObject, UObject* Object, const FClassInfo& Info, const FHandoverChangeState& Changes, bool bIsInitialData, TArray<Schema_FieldId>* ClearedIds /* = nullptr */)
{
	bool bWroteSomething = false;

	for (uint16 ChangedHandle : Changes)
	{
		check(ChangedHandle > 0 && ChangedHandle - 1 < Info.HandoverProperties.Num());
		const FHandoverPropertyInfo& PropertyInfo = Info.HandoverProperties[ChangedHandle - 1];

		const uint8* Data = (uint8*)Object + PropertyInfo.Offset;
		FUnresolvedObjectsSet UnresolvedObjects;

		AddProperty(ComponentObject, ChangedHandle, PropertyInfo.Property, Data, UnresolvedObjects, ClearedIds);

		if (UnresolvedObjects.Num() == 0)
		{
			bWroteSomething = true;
		}
		else
		{
			if (!bIsInitialData)
			{
				// Don't send updates for fields with unresolved objects, unless it's the initial data,
				// in which case all fields should be populated.
				Schema_ClearField(ComponentObject, ChangedHandle);
			}

			PendingHandoverUnresolvedObjectsMap.Add(ChangedHandle, UnresolvedObjects);
		}
	}

	return bWroteSomething;
}

void ComponentFactory::AddProperty(Schema_Object* Object, Schema_FieldId FieldId, UProperty* Property, const uint8* Data, TSet<TWeakObjectPtr<const UObject>>& UnresolvedObjects, TArray<Schema_FieldId>* ClearedIds)
{
	if (UStructProperty* StructProperty = Cast<UStructProperty>(Property))
	{
		UScriptStruct* Struct = StructProperty->Struct;
		FSpatialNetBitWriter ValueDataWriter(PackageMap, UnresolvedObjects);
		bool bHasUnmapped = false;

		if (Struct->StructFlags & STRUCT_NetSerializeNative)
		{
			UScriptStruct::ICppStructOps* CppStructOps = Struct->GetCppStructOps();
			check(CppStructOps); // else should not have STRUCT_NetSerializeNative
			bool bSuccess = true;
			if (!CppStructOps->NetSerialize(ValueDataWriter, PackageMap, bSuccess, const_cast<uint8*>(Data)))
			{
				bHasUnmapped = true;
			}

			// Check the success of the serialization and print a warning if it failed. This is how native handles failed serialization.
			if (!bSuccess)
			{
				UE_LOG(LogSpatialNetSerialize, Warning, TEXT("AddProperty: NetSerialize %s failed."), *Struct->GetFullName());
				return;
			}
		}
		else
		{
			TSharedPtr<FRepLayout> RepLayout = NetDriver->GetStructRepLayout(Struct);

			RepLayout_SerializePropertiesForStruct(*RepLayout, ValueDataWriter, PackageMap, const_cast<uint8*>(Data), bHasUnmapped);
		}

		AddBytesToSchema(Object, FieldId, ValueDataWriter);
	}
	else if (UBoolProperty* BoolProperty = Cast<UBoolProperty>(Property))
	{
		Schema_AddBool(Object, FieldId, (uint8)BoolProperty->GetPropertyValue(Data));
	}
	else if (UFloatProperty* FloatProperty = Cast<UFloatProperty>(Property))
	{
		Schema_AddFloat(Object, FieldId, FloatProperty->GetPropertyValue(Data));
	}
	else if (UDoubleProperty* DoubleProperty = Cast<UDoubleProperty>(Property))
	{
		Schema_AddDouble(Object, FieldId, DoubleProperty->GetPropertyValue(Data));
	}
	else if (UInt8Property* Int8Property = Cast<UInt8Property>(Property))
	{
		Schema_AddInt32(Object, FieldId, (int32)Int8Property->GetPropertyValue(Data));
	}
	else if (UInt16Property* Int16Property = Cast<UInt16Property>(Property))
	{
		Schema_AddInt32(Object, FieldId, (int32)Int16Property->GetPropertyValue(Data));
	}
	else if (UIntProperty* IntProperty = Cast<UIntProperty>(Property))
	{
		Schema_AddInt32(Object, FieldId, IntProperty->GetPropertyValue(Data));
	}
	else if (UInt64Property* Int64Property = Cast<UInt64Property>(Property))
	{
		Schema_AddInt64(Object, FieldId, Int64Property->GetPropertyValue(Data));
	}
	else if (UByteProperty* ByteProperty = Cast<UByteProperty>(Property))
	{
		Schema_AddUint32(Object, FieldId, (uint32)ByteProperty->GetPropertyValue(Data));
	}
	else if (UUInt16Property* UInt16Property = Cast<UUInt16Property>(Property))
	{
		Schema_AddUint32(Object, FieldId, (uint32)UInt16Property->GetPropertyValue(Data));
	}
	else if (UUInt32Property* UInt32Property = Cast<UUInt32Property>(Property))
	{
		Schema_AddUint32(Object, FieldId, UInt32Property->GetPropertyValue(Data));
	}
	else if (UUInt64Property* UInt64Property = Cast<UUInt64Property>(Property))
	{
		Schema_AddUint64(Object, FieldId, UInt64Property->GetPropertyValue(Data));
	}
	else if (UObjectPropertyBase* ObjectProperty = Cast<UObjectPropertyBase>(Property))
	{
		FUnrealObjectRef ObjectRef = FUnrealObjectRef::NULL_OBJECT_REF;

		UObject* ObjectValue = ObjectProperty->GetObjectPropertyValue(Data);

		if (ObjectValue != nullptr && !ObjectValue->IsPendingKill())
		{
			FNetworkGUID NetGUID;
			if (ObjectValue->IsSupportedForNetworking())
			{
				NetGUID = PackageMap->GetNetGUIDFromObject(ObjectValue);

				if (!NetGUID.IsValid())
				{
					if (ObjectValue->IsFullNameStableForNetworking())
					{
						NetGUID = PackageMap->ResolveStablyNamedObject(ObjectValue);
					}
					else
					{
						NetGUID = PackageMap->TryResolveObjectAsEntity(ObjectValue);
					}
				}
			}

			// The secondary part of the check is needed if we couldn't assign an entity id (e.g. ran out of entity ids)
			if (NetGUID.IsValid() || (ObjectValue->IsSupportedForNetworking() && !ObjectValue->IsFullNameStableForNetworking()))
			{
				ObjectRef = PackageMap->GetUnrealObjectRefFromNetGUID(NetGUID);
			}
			else
			{
				ObjectRef = FUnrealObjectRef::NULL_OBJECT_REF;
			}

			if (ObjectRef == FUnrealObjectRef::UNRESOLVED_OBJECT_REF)
			{
				// There are cases where something assigned a NetGUID without going through the FSpatialNetGUID (e.g. FObjectReplicator)
				// Assign an UnrealObjectRef by going through the FSpatialNetGUID flow
				if (ObjectValue->IsFullNameStableForNetworking())
				{
					PackageMap->ResolveStablyNamedObject(ObjectValue);
					ObjectRef = PackageMap->GetUnrealObjectRefFromNetGUID(NetGUID);
				}
				else
				{
					UnresolvedObjects.Add(ObjectValue);
					ObjectRef = FUnrealObjectRef::NULL_OBJECT_REF;
				}
			}
		}

		if (ObjectProperty->PropertyFlags & CPF_AlwaysInterested)
		{
			bInterestHasChanged = true;
		}

		AddObjectRefToSchema(Object, FieldId, ObjectRef);
	}
	else if (UNameProperty* NameProperty = Cast<UNameProperty>(Property))
	{
		AddStringToSchema(Object, FieldId, NameProperty->GetPropertyValue(Data).ToString());
	}
	else if (UStrProperty* StrProperty = Cast<UStrProperty>(Property))
	{
		AddStringToSchema(Object, FieldId, StrProperty->GetPropertyValue(Data));
	}
	else if (UTextProperty* TextProperty = Cast<UTextProperty>(Property))
	{
		AddStringToSchema(Object, FieldId, TextProperty->GetPropertyValue(Data).ToString());
	}
	else if (UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Property))
	{
		FScriptArrayHelper ArrayHelper(ArrayProperty, Data);
		for (int i = 0; i < ArrayHelper.Num(); i++)
		{
			AddProperty(Object, FieldId, ArrayProperty->Inner, ArrayHelper.GetRawPtr(i), UnresolvedObjects, ClearedIds);
		}

		if (ArrayHelper.Num() == 0 && ClearedIds)
		{
			ClearedIds->Add(FieldId);
		}
	}
	else if (UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
	{
		if (EnumProperty->ElementSize < 4)
		{
			Schema_AddUint32(Object, FieldId, (uint32)EnumProperty->GetUnderlyingProperty()->GetUnsignedIntPropertyValue(Data));
		}
		else
		{
			AddProperty(Object, FieldId, EnumProperty->GetUnderlyingProperty(), Data, UnresolvedObjects, ClearedIds);
		}
	}
	else if (Property->IsA<UDelegateProperty>() || Property->IsA<UMulticastDelegateProperty>() || Property->IsA<UInterfaceProperty>())
	{
		// These properties can be set to replicate, but won't serialize across the network.
	}
	else
	{
		checkf(false, TEXT("Tried to add unknown property in field %d"), FieldId);
	}
}

TArray<Worker_ComponentData> ComponentFactory::CreateComponentDatas(UObject* Object, const FClassInfo& Info, const FRepChangeState& RepChangeState, const FHandoverChangeState& HandoverChangeState)
{
	TArray<Worker_ComponentData> ComponentDatas;

	if (Info.SchemaComponents[SCHEMA_Data] != SpatialConstants::INVALID_COMPONENT_ID)
	{
		ComponentDatas.Add(CreateComponentData(Info.SchemaComponents[SCHEMA_Data], Object, RepChangeState, SCHEMA_Data));
	}

	if (Info.SchemaComponents[SCHEMA_OwnerOnly] != SpatialConstants::INVALID_COMPONENT_ID)
	{
		ComponentDatas.Add(CreateComponentData(Info.SchemaComponents[SCHEMA_OwnerOnly], Object, RepChangeState, SCHEMA_OwnerOnly));
	}

	if (Info.SchemaComponents[SCHEMA_Handover] != SpatialConstants::INVALID_COMPONENT_ID)
	{
		ComponentDatas.Add(CreateHandoverComponentData(Info.SchemaComponents[SCHEMA_Handover], Object, Info, HandoverChangeState));
	}

	return ComponentDatas;
}

Worker_ComponentData ComponentFactory::CreateComponentData(Worker_ComponentId ComponentId, UObject* Object, const FRepChangeState& Changes, ESchemaComponentType PropertyGroup)
{
	Worker_ComponentData ComponentData = {};
	ComponentData.component_id = ComponentId;
	ComponentData.schema_type = Schema_CreateComponentData(ComponentId);
	Schema_Object* ComponentObject = Schema_GetComponentDataFields(ComponentData.schema_type);

	// We're currently ignoring ClearedId fields, which is problematic if the initial replicated state
	// is different to what the default state is (the client will have the incorrect data). UNR:959
	FillSchemaObject(ComponentObject, Object, Changes, PropertyGroup, true);

	return ComponentData;
}

Worker_ComponentData ComponentFactory::CreateEmptyComponentData(Worker_ComponentId ComponentId)
{
	Worker_ComponentData ComponentData = {};
	ComponentData.component_id = ComponentId;
	ComponentData.schema_type = Schema_CreateComponentData(ComponentId);

	return ComponentData;
}

Worker_ComponentData ComponentFactory::CreateHandoverComponentData(Worker_ComponentId ComponentId, UObject* Object, const FClassInfo& Info, const FHandoverChangeState& Changes)
{
	Worker_ComponentData ComponentData = CreateEmptyComponentData(ComponentId);
	Schema_Object* ComponentObject = Schema_GetComponentDataFields(ComponentData.schema_type);

	FillHandoverSchemaObject(ComponentObject, Object, Info, Changes, true);

	return ComponentData;
}

TArray<Worker_ComponentUpdate> ComponentFactory::CreateComponentUpdates(UObject* Object, const FClassInfo& Info, Worker_EntityId EntityId, const FRepChangeState* RepChangeState, const FHandoverChangeState* HandoverChangeState)
{
	TArray<Worker_ComponentUpdate> ComponentUpdates;

	if (RepChangeState)
	{
		if (Info.SchemaComponents[SCHEMA_Data] != SpatialConstants::INVALID_COMPONENT_ID)
		{
			bool bWroteSomething = false;
			Worker_ComponentUpdate MultiClientUpdate = CreateComponentUpdate(Info.SchemaComponents[SCHEMA_Data], Object, *RepChangeState, SCHEMA_Data, bWroteSomething);
			if (bWroteSomething)
			{
				ComponentUpdates.Add(MultiClientUpdate);
			}
		}

		if (Info.SchemaComponents[SCHEMA_OwnerOnly] != SpatialConstants::INVALID_COMPONENT_ID)
		{
			bool bWroteSomething = false;
			Worker_ComponentUpdate SingleClientUpdate = CreateComponentUpdate(Info.SchemaComponents[SCHEMA_OwnerOnly], Object, *RepChangeState, SCHEMA_OwnerOnly, bWroteSomething);
			if (bWroteSomething)
			{
				ComponentUpdates.Add(SingleClientUpdate);
			}
		}
	}

	if (HandoverChangeState)
	{
		if (Info.SchemaComponents[SCHEMA_Handover] != SpatialConstants::INVALID_COMPONENT_ID)
		{
			bool bWroteSomething = false;
			Worker_ComponentUpdate HandoverUpdate = CreateHandoverComponentUpdate(Info.SchemaComponents[SCHEMA_Handover], Object, Info, *HandoverChangeState, bWroteSomething);
			if (bWroteSomething)
			{
				ComponentUpdates.Add(HandoverUpdate);
			}
		}
	}

	// Only support Interest for Actors for now.
	if (Object->IsA<AActor>() && bInterestHasChanged)
	{
		InterestFactory InterestUpdateFactory(Cast<AActor>(Object), Info, NetDriver);
		ComponentUpdates.Add(InterestUpdateFactory.CreateInterestUpdate());
	}

	return ComponentUpdates;
}

Worker_ComponentUpdate ComponentFactory::CreateComponentUpdate(Worker_ComponentId ComponentId, UObject* Object, const FRepChangeState& Changes, ESchemaComponentType PropertyGroup, bool& bWroteSomething)
{
	Worker_ComponentUpdate ComponentUpdate = {};

	ComponentUpdate.component_id = ComponentId;
	ComponentUpdate.schema_type = Schema_CreateComponentUpdate(ComponentId);
	Schema_Object* ComponentObject = Schema_GetComponentUpdateFields(ComponentUpdate.schema_type);

	TArray<Schema_FieldId> ClearedIds;

	bWroteSomething = FillSchemaObject(ComponentObject, Object, Changes, PropertyGroup, false, &ClearedIds);

	for (Schema_FieldId Id : ClearedIds)
	{
		Schema_AddComponentUpdateClearedField(ComponentUpdate.schema_type, Id);
	}

	if (!bWroteSomething)
	{
		Schema_DestroyComponentUpdate(ComponentUpdate.schema_type);
	}

	return ComponentUpdate;
}

Worker_ComponentUpdate ComponentFactory::CreateHandoverComponentUpdate(Worker_ComponentId ComponentId, UObject* Object, const FClassInfo& Info, const FHandoverChangeState& Changes, bool& bWroteSomething)
{
	Worker_ComponentUpdate ComponentUpdate = {};

	ComponentUpdate.component_id = ComponentId;
	ComponentUpdate.schema_type = Schema_CreateComponentUpdate(ComponentId);
	Schema_Object* ComponentObject = Schema_GetComponentUpdateFields(ComponentUpdate.schema_type);

	TArray<Schema_FieldId> ClearedIds;

	bWroteSomething = FillHandoverSchemaObject(ComponentObject, Object, Info, Changes, false, &ClearedIds);

	for (Schema_FieldId Id : ClearedIds)
	{
		Schema_AddComponentUpdateClearedField(ComponentUpdate.schema_type, Id);
	}

	if (!bWroteSomething)
	{
		Schema_DestroyComponentUpdate(ComponentUpdate.schema_type);
	}

	return ComponentUpdate;
}

} // namespace SpatialGDK
