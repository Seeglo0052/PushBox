// Copyright PushBox Game. All Rights Reserved.

#include "PushBoxEditor.h"
#include "PuzzleEditorCommands.h"
#include "PuzzleEditorStyle.h"
#include "SPuzzleEditorWidget.h"
#include "ToolMenus.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "LevelEditor.h"

#define LOCTEXT_NAMESPACE "FPushBoxEditorModule"

static const FName PuzzleEditorTabName("PuzzleEditor");

void FPushBoxEditorModule::StartupModule()
{
	// Initialize style and commands
	FPuzzleEditorStyle::Initialize();
	FPuzzleEditorStyle::ReloadTextures();
	FPuzzleEditorCommands::Register();

	// Bind commands
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FPuzzleEditorCommands::Get().OpenPuzzleEditor,
		FExecuteAction::CreateRaw(this, &FPushBoxEditorModule::RegisterMenuExtensions),
		FCanExecuteAction());

	// Register the tab spawner
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(PuzzleEditorTabName,
		FOnSpawnTab::CreateRaw(this, &FPushBoxEditorModule::OnSpawnPuzzleEditorTab))
		.SetDisplayName(LOCTEXT("PuzzleEditorTabTitle", "Puzzle Editor"))
		.SetTooltipText(LOCTEXT("PuzzleEditorTabTooltip", "Open the PushBox Puzzle Editor"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
		.SetIcon(FSlateIcon(FPuzzleEditorStyle::GetStyleSetName(), "PushBoxEditor.OpenPuzzleEditor"));

	// Extend the main toolbar
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([this]()
	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PushBox");
		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			"OpenPuzzleEditor",
			FUIAction(FExecuteAction::CreateLambda([]()
			{
				FGlobalTabmanager::Get()->TryInvokeTab(PuzzleEditorTabName);
			})),
			LOCTEXT("PuzzleEditorButton", "Puzzle Editor"),
			LOCTEXT("PuzzleEditorButtonTooltip", "Open the PushBox Puzzle Editor"),
			FSlateIcon(FPuzzleEditorStyle::GetStyleSetName(), "PushBoxEditor.OpenPuzzleEditor")
		));

		// Extend Content Browser context menu
		UToolMenu* ContentBrowserMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu");
		FToolMenuSection& CBSection = ContentBrowserMenu->FindOrAddSection("PushBoxActions");
		CBSection.AddMenuEntry(
			"CreatePuzzleLevel",
			LOCTEXT("CreatePuzzleLevel", "Create Puzzle Level Data"),
			LOCTEXT("CreatePuzzleLevelTooltip", "Create a new PuzzleLevelDataAsset"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]()
			{
				// This will be handled by the PuzzleEditorSubsystem
				FGlobalTabmanager::Get()->TryInvokeTab(PuzzleEditorTabName);
			}))
		);

		// Extend Actor context menu in World Outliner
		UToolMenu* ActorMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.ActorContextMenu");
		FToolMenuSection& ActorSection = ActorMenu->FindOrAddSection("PushBoxActor");
		ActorSection.AddMenuEntry(
			"SetupAsPushBoxLevel",
			LOCTEXT("SetupAsPushBoxLevel", "Setup as PushBox Level"),
			LOCTEXT("SetupAsPushBoxLevelTooltip", "Configure selected WFC actor as a PushBox puzzle level"),
			FSlateIcon(),
			FUIAction()
		);
	}));
}

void FPushBoxEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	FPuzzleEditorCommands::Unregister();
	FPuzzleEditorStyle::Shutdown();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PuzzleEditorTabName);
}

void FPushBoxEditorModule::RegisterMenuExtensions()
{
	FGlobalTabmanager::Get()->TryInvokeTab(PuzzleEditorTabName);
}

TSharedRef<SDockTab> FPushBoxEditorModule::OnSpawnPuzzleEditorTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SPuzzleEditorWidget)
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPushBoxEditorModule, PushBoxEditor)
