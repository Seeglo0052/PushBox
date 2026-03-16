// Copyright PushBox Game. All Rights Reserved.

#include "PuzzleEditorCommands.h"

#define LOCTEXT_NAMESPACE "FPuzzleEditorCommands"

void FPuzzleEditorCommands::RegisterCommands()
{
	UI_COMMAND(OpenPuzzleEditor,
		"Puzzle Editor",
		"Open the PushBox Puzzle Editor",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::P));

	UI_COMMAND(GenerateTerrain,
		"Generate Terrain",
		"Run WFC to generate puzzle terrain",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::G));

	UI_COMMAND(SavePuzzle,
		"Save Puzzle",
		"Save current puzzle to DataAsset",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::S));

	UI_COMMAND(ValidatePuzzle,
		"Validate Puzzle",
		"Validate that the puzzle is solvable",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::V));

	UI_COMMAND(ModeTilePlacement,
		"Tile Mode",
		"Switch to tile placement mode",
		EUserInterfaceActionType::RadioButton,
		FInputChord(EKeys::One));

	UI_COMMAND(ModeGoalState,
		"Goal Mode",
		"Switch to goal state editing mode",
		EUserInterfaceActionType::RadioButton,
		FInputChord(EKeys::Two));

	UI_COMMAND(ModeReverseEdit,
		"Reverse Mode",
		"Switch to reverse editing mode (pull boxes)",
		EUserInterfaceActionType::RadioButton,
		FInputChord(EKeys::Three));

	UI_COMMAND(ModePlayTest,
		"PlayTest Mode",
		"Switch to play-test mode (forward push)",
		EUserInterfaceActionType::RadioButton,
		FInputChord(EKeys::Four));

	UI_COMMAND(EditorUndo,
		"Undo",
		"Undo last editor action",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::Z));

	UI_COMMAND(EditorRedo,
		"Redo",
		"Redo last editor action",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::Y));
}

#undef LOCTEXT_NAMESPACE
