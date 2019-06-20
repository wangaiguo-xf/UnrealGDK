// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "Async/Future.h"
#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSpatialGDKEditor, Log, All);

DECLARE_DELEGATE_OneParam(FSpatialGDKEditorErrorHandler, FString);

class SPATIALGDKEDITOR_API FSpatialGDKEditor
{
public:
	FSpatialGDKEditor() : bSchemaGeneratorRunning(false)
	{
	}

	bool GenerateSchema(bool bFullScan);
	void GenerateSnapshot(UWorld* World, FString SnapshotFilename, FSimpleDelegate SuccessCallback, FSimpleDelegate FailureCallback, FSpatialGDKEditorErrorHandler ErrorCallback);

	bool IsSchemaGeneratorRunning() { return bSchemaGeneratorRunning; }
	bool FullScanRequired();

private:
	bool bSchemaGeneratorRunning;
	TFuture<bool> SchemaGeneratorResult;

	bool LoadPotentialAssets(TArray<TStrongObjectPtr<UObject>>& OutAssets);

	FDelegateHandle OnAssetLoadedHandle;
	void OnAssetLoaded(UObject* Asset);
	void RemoveEditorAssetLoadedCallback();
};
