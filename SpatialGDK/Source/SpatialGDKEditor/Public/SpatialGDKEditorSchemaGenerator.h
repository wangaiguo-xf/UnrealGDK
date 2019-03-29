// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "Logging/LogMacros.h"
#include "UObject/StrongObjectPtr.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSpatialGDKSchemaGenerator, Log, All);

SPATIALGDKEDITOR_API bool SpatialGDKGenerateSchema(TArray<FAssetData> WorldAssets);

SPATIALGDKEDITOR_API void TryLoadExistingSchemaDatabase();

SPATIALGDKEDITOR_API void PreProcessSchemaMap();

SPATIALGDKEDITOR_API void LoadDefaultGameModes();
