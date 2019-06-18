// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "SpatialGDKEditorToolbarCommands.h"

#define LOCTEXT_NAMESPACE "FSpatialGDKEditorToolbarModule"

void FSpatialGDKEditorToolbarCommands::RegisterCommands()
{
	UI_COMMAND(CreateSpatialGDKSchema, "Schema", "Creates SpatialOS Unreal GDK schema for assets in memory.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(CreateSpatialGDKSchemaFull, "Schema (Full Scan)", "Creates SpatialOS Unreal GDK schema for all assets.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(CreateSpatialGDKSnapshot, "Snapshot", "Creates SpatialOS Unreal GDK snapshot.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(StartSpatialDeployment, "Start Deployment", "Starts a local instance of SpatialOS.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(StopSpatialDeployment, "Stop Deployment", "Stops SpatialOS.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(LaunchInspectorWebPageAction, "Inspector", "Launches default web browser to SpatialOS Inspector.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(StartSpatialService, "Service Stopped", "Starts the Spatial service daemon.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(StopSpatialService, "Service Running", "Stops the Spatial service daemon.", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE
