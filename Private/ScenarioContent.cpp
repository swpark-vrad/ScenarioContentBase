// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScenarioContent.h"
#include "Global/ScenarioGameplayTags.h"

#define LOCTEXT_NAMESPACE "FScenarioContentModule"

void FScenarioContentModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	// 시나리오 기본 태그 추가
	FScenarioGameplayTags::InitializeNativeTags();
}

void FScenarioContentModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FScenarioContentModule, ScenarioContent)