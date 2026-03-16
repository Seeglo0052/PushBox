// Copyright PushBox Game. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * PushBoxEditor module - provides the puzzle editor tools for designers.
 */
class FPushBoxEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenuExtensions();

	TSharedRef<class SDockTab> OnSpawnPuzzleEditorTab(const class FSpawnTabArgs& SpawnTabArgs);

	TSharedPtr<class FUICommandList> PluginCommands;
};
