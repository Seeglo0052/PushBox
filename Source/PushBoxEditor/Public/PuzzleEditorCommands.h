// Copyright PushBox Game. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "PuzzleEditorStyle.h"

/**
 * Custom editor commands and hotkeys for the PushBox puzzle editor.
 */
class FPuzzleEditorCommands : public TCommands<FPuzzleEditorCommands>
{
public:
	FPuzzleEditorCommands()
		: TCommands<FPuzzleEditorCommands>(
			TEXT("PushBoxEditor"),
			NSLOCTEXT("Contexts", "PushBoxEditor", "PushBox Puzzle Editor"),
			NAME_None,
			FPuzzleEditorStyle::GetStyleSetName())
	{
	}

	virtual void RegisterCommands() override;

	/** Open the puzzle editor tab */
	TSharedPtr<FUICommandInfo> OpenPuzzleEditor;

	/** Generate terrain */
	TSharedPtr<FUICommandInfo> GenerateTerrain;

	/** Save current puzzle */
	TSharedPtr<FUICommandInfo> SavePuzzle;

	/** Validate puzzle solvability */
	TSharedPtr<FUICommandInfo> ValidatePuzzle;

	/** Switch to tile placement mode */
	TSharedPtr<FUICommandInfo> ModeTilePlacement;

	/** Switch to goal state mode */
	TSharedPtr<FUICommandInfo> ModeGoalState;

	/** Switch to reverse edit mode */
	TSharedPtr<FUICommandInfo> ModeReverseEdit;

	/** Switch to playtest mode */
	TSharedPtr<FUICommandInfo> ModePlayTest;

	/** Undo last editor action */
	TSharedPtr<FUICommandInfo> EditorUndo;

	/** Redo last editor action */
	TSharedPtr<FUICommandInfo> EditorRedo;
};
