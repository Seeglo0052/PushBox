// Copyright PushBox Game. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "PushBoxTypes.h"
#include "MCTSPuzzleGenerator.h"

class UPuzzleEditorSubsystem;
class SUniformGridPanel;

/**
 * Main Slate widget for the PushBox Puzzle Editor tab.
 * Shows a grid preview and provides controls for the reverse-design workflow.
 */
class SPuzzleEditorWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPuzzleEditorWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// SWidget interface ！ used for drag-brush painting
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	// Layout builders
	TSharedRef<SWidget> BuildToolbar();
	TSharedRef<SWidget> BuildPropertiesPanel();
	TSharedRef<SWidget> BuildStatusBar();

	// Rebuild the grid panel to match current GridSize
	void RebuildGrid();

	// Build cell content widget (changes based on mode)
	TSharedRef<SWidget> BuildCellWidget(int32 X, int32 Y);

	// Build the directional arrow overlay for a box cell in reverse mode
	TSharedRef<SWidget> BuildReverseArrowsForBox(int32 X, int32 Y);

	// Build the directional arrow overlay for the player cell in reverse mode
	TSharedRef<SWidget> BuildReverseArrowsForPlayer(int32 X, int32 Y);

	// Build NSEW direction buttons for a OneWayDoor cell
	TSharedRef<SWidget> BuildDirectionArrowsForDoor(int32 X, int32 Y);

	// Grid cell rendering (delegates ！ called every frame)
	FSlateColor GetCellColor(int32 X, int32 Y) const;
	FText GetCellLabel(int32 X, int32 Y) const;
	FReply OnCellClicked(int32 X, int32 Y);

	/** Apply brush/eraser to a cell. Called on click and on drag-enter. */
	void ApplyBrushToCell(int32 X, int32 Y);

	// Reverse mode: arrow clicked for a specific box
	FReply OnReversePullArrow(int32 X, int32 Y, EPushBoxDirection Dir);

	// Reverse mode: arrow clicked to move the player
	FReply OnReversePlayerMove(int32 X, int32 Y, EPushBoxDirection Dir);

	// OneWayDoor direction changed
	FReply OnDoorDirectionChanged(int32 X, int32 Y, EPushBoxDirection Dir);

	// Button style helper
	FSlateColor GetModeButtonColor(EPuzzleEditorMode Mode) const;
	FSlateColor GetTileBrushButtonColor(EPushBoxTileType Type) const;
	FSlateColor GetDirectionButtonColor(EPushBoxDirection Dir) const;
	/** Highlight color for the eraser button */
	FSlateColor GetEraserButtonColor() const;

	// Toolbar actions
	FReply OnSave();
	FReply OnMCTSAutoGenerate();
	FReply OnClearGrid();

	// Mode switching
	void OnModeChanged(EPuzzleEditorMode NewMode);

	// Properties
	FText GetGridSizeText() const;
	FText GetMoveCountText() const;
	FText GetStatusText() const;
	FText GetModeText() const;

	// WFC asset picker helpers
	FString GetWFCAssetPath() const;
	void OnWFCAssetChanged(const FAssetData& AssetData);
	FText GetWFCAssetInfoText() const;

	// Puzzle DataAsset picker helpers
	FString GetPuzzleDataAssetPath() const;
	void OnPuzzleDataAssetChanged(const FAssetData& AssetData);

	// State
	UPuzzleEditorSubsystem* GetSubsystem() const;

	// Cached
	FIntPoint PendingGridSize = FIntPoint(10, 10);
	FText StatusMessage;

	EPushBoxTileType SelectedTileBrush = EPushBoxTileType::Wall;
	EPushBoxDirection SelectedOneWayDirection = EPushBoxDirection::North;
	FMCTSConfig MCTSConfig;

	/** When true, the eraser tool is active ！ clicking/dragging resets cells to Empty */
	bool bEraserActive = false;

	/** True while LMB is held down on the grid for drag-painting */
	bool bIsDragging = false;

	/** Last cell painted during a drag (to avoid repeated paints on the same cell) */
	FIntPoint LastDragCell = FIntPoint(-1, -1);

	/** The box holding the grid panel ！ swapped out on RebuildGrid */
	TSharedPtr<SBox> GridContainer;

	/** Track current grid dimensions to detect when rebuild is needed */
	FIntPoint CurrentDisplayedGridSize = FIntPoint::ZeroValue;

	/** Cell size in pixels for square display */
	static constexpr float CellSizePx = 100.0f;
};
