// Copyright PushBox Game. All Rights Reserved.

#include "SPuzzleEditorWidget.h"
#include "PuzzleEditorSubsystem.h"
#include "PuzzleEditorStyle.h"
#include "Grid/PushBoxGridManager.h"
#include "Data/PuzzleLevelDataAsset.h"
#include "Utilities/PushBoxFunctionLibrary.h"
#include "WFCAsset.h"
#include "Core/Grids/WFCGrid2D.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "PropertyCustomizationHelpers.h"
#include "Misc/MessageDialog.h"
#include "Misc/ScopedSlowTask.h"
#include "Framework/Application/SlateApplication.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "SPuzzleEditorWidget"

void SPuzzleEditorWidget::Construct(const FArguments& InArgs)
{
	GridContainer = SNew(SBox);

	// Try to restore last session ！ triggers grid rebuild if data is available
	if (UPuzzleEditorSubsystem* Sub = GetSubsystem())
	{
		UPushBoxGridManager* GM = Sub->GetActiveGridManager();
		if (GM && GM->GridSize.X > 0 && GM->GridSize.Y > 0)
		{
			PendingGridSize = GM->GridSize;
		}
	}

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(4)
		[ BuildToolbar() ]

		+ SVerticalBox::Slot().AutoHeight()
		[ SNew(SSeparator) ]

		+ SVerticalBox::Slot().FillHeight(1.0f).Padding(4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(3.0f).Padding(4)
			[
				SNew(SScrollBox)
				.Orientation(Orient_Vertical)
				+ SScrollBox::Slot()
				[
					SNew(SScrollBox)
					.Orientation(Orient_Horizontal)
					+ SScrollBox::Slot()
					[ GridContainer.ToSharedRef() ]
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(4)
			[ BuildPropertiesPanel() ]
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(4)
		[ BuildStatusBar() ]
	];

	// Build initial grid if data was restored
	if (UPuzzleEditorSubsystem* Sub = GetSubsystem())
	{
		if (Sub->GetActiveGridManager())
		{
			RebuildGrid();
		}
	}
}

// ------------------------------------------------------------------
// Tick ！ drag-brush painting
// ------------------------------------------------------------------

void SPuzzleEditorWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (!FSlateApplication::IsInitialized()) return;

	bool bLMBDown = FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::LeftMouseButton);

	// When LMB is released after dragging, do a final rebuild to sync widget types
	if (!bLMBDown)
	{
		if (bIsDragging)
		{
			bIsDragging = false;
			LastDragCell = FIntPoint(-1, -1);
			RebuildGrid();
		}
		return;
	}

	// Only do drag-brush in TilePlacement mode
	UPuzzleEditorSubsystem* Sub = GetSubsystem();
	if (!Sub || Sub->GetEditMode() != EPuzzleEditorMode::TilePlacement)
		return;

	if (!GridContainer.IsValid() || CurrentDisplayedGridSize.X <= 0)
		return;

	// Get mouse position relative to the grid container
	FGeometry GridGeo = GridContainer->GetCachedGeometry();
	FVector2D MouseScreenPos = FSlateApplication::Get().GetCursorPos();
	FVector2D LocalPos = GridGeo.AbsoluteToLocal(MouseScreenPos);

	// Each cell slot is CellSizePx + 2px padding (1px each side from SlotPadding(1))
	const float SlotSize = CellSizePx + 2.f;
	int32 Col = FMath::FloorToInt32(static_cast<float>(LocalPos.X / SlotSize));
	int32 Row = FMath::FloorToInt32(static_cast<float>(LocalPos.Y / SlotSize));

	int32 SX = CurrentDisplayedGridSize.X;
	int32 SY = CurrentDisplayedGridSize.Y;
	if (Col < 0 || Col >= SX || Row < 0 || Row >= SY)
		return;

	// Reverse the mirroring: Grid X = SizeX-1-Col, Grid Y = SizeY-1-Row
	int32 GridX = SX - 1 - Col;
	int32 GridY = SY - 1 - Row;

	FIntPoint CellCoord(GridX, GridY);
	if (CellCoord != LastDragCell)
	{
		bIsDragging = true;
		LastDragCell = CellCoord;
		ApplyBrushToCell(GridX, GridY);
		// No RebuildGrid here ！ GetCellColor/GetCellLabel are delegate-bound
		// and will update automatically. RebuildGrid on mouse release.
	}
}

// ------------------------------------------------------------------
// Dynamic grid rebuild
// ------------------------------------------------------------------

void SPuzzleEditorWidget::RebuildGrid()
{
	UPushBoxGridManager* GM = nullptr;
	if (UPuzzleEditorSubsystem* Sub = GetSubsystem())
		GM = Sub->GetActiveGridManager();

	int32 SizeX = GM ? GM->GridSize.X : PendingGridSize.X;
	int32 SizeY = GM ? GM->GridSize.Y : PendingGridSize.Y;

	if (SizeX <= 0 || SizeY <= 0)
	{
		SizeX = PendingGridSize.X;
		SizeY = PendingGridSize.Y;
	}

	CurrentDisplayedGridSize = FIntPoint(SizeX, SizeY);

	// Build grid with square cells using fixed-size SBox for each cell.
	// X axis: mirror so that grid X=0 is on the RIGHT (matching UE's top-down view
	// where the default camera looks down -Z with +X going right visually,
	// but the grid's column 0 should map to the visual left of the level).
	TSharedRef<SUniformGridPanel> GridPanel = SNew(SUniformGridPanel).SlotPadding(FMargin(1));

	for (int32 Y = SizeY - 1; Y >= 0; --Y)
	{
		for (int32 X = 0; X < SizeX; ++X)
		{
			// Mirror X axis: column 0 = highest X, column (SizeX-1) = X=0
			// This ensures the editor layout matches the in-game top-down view
			const int32 Col = SizeX - 1 - X;
			const int32 Row = SizeY - 1 - Y;

			GridPanel->AddSlot(Col, Row)
			[
				SNew(SBox)
				.WidthOverride(CellSizePx)
				.HeightOverride(CellSizePx)
				[
					BuildCellWidget(X, Y)
				]
			];
		}
	}

	GridContainer->SetContent(GridPanel);
}

TSharedRef<SWidget> SPuzzleEditorWidget::BuildCellWidget(int32 X, int32 Y)
{
	int32 CX = X, CY = Y;

	UPuzzleEditorSubsystem* Sub = GetSubsystem();
	UPushBoxGridManager* GM = Sub ? Sub->GetActiveGridManager() : nullptr;

	if (GM && Sub)
	{
		EPuzzleEditorMode Mode = Sub->GetEditMode();

		if (Mode == EPuzzleEditorMode::ReverseEdit)
		{
			FIntPoint Coord(X, Y);
			bool bHasBox = GM->HasBoxAt(Coord);
			bool bHasPlayer = (GM->PlayerGridPosition == Coord);

			if (bHasBox && bHasPlayer)
			{
				// Player and box overlap ！ prioritize player movement
				// so the user can move the player away from the box.
				return BuildReverseArrowsForPlayer(X, Y);
			}
			if (bHasBox)
			{
				return BuildReverseArrowsForBox(X, Y);
			}
			if (bHasPlayer)
			{
				return BuildReverseArrowsForPlayer(X, Y);
			}
		}

		if (Mode == EPuzzleEditorMode::TilePlacement)
		{
			// OneWayDoor cells get direction arrows for quick direction change
			FPushBoxCellData Cell = GM->GetCellData(FIntPoint(X, Y));
			if (Cell.TileType == EPushBoxTileType::OneWayDoor)
			{
				return BuildDirectionArrowsForDoor(X, Y);
			}
		}
	}

	// Normal cell: single click handled by OnCellClicked.
	// Drag painting is handled by the grid-level overlay in RebuildGrid.
	return SNew(SButton)
		.ButtonColorAndOpacity(this, &SPuzzleEditorWidget::GetCellColor, X, Y)
		.ContentPadding(FMargin(0))
		.OnClicked_Lambda([this, CX, CY]() { return OnCellClicked(CX, CY); })
		[
			SNew(STextBlock)
			.Text(this, &SPuzzleEditorWidget::GetCellLabel, X, Y)
			.Justification(ETextJustify::Center)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
		];
}

TSharedRef<SWidget> SPuzzleEditorWidget::BuildReverseArrowsForBox(int32 X, int32 Y)
{
	int32 CX = X, CY = Y;

	auto ArrowBtn = [this, CX, CY](const FText& Label, EPushBoxDirection Dir)
	{
		return SNew(SButton)
			.Text(Label)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ButtonColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.3f, 0.6f, 0.9f)))
			.ContentPadding(FMargin(0))
			.OnClicked_Lambda([this, CX, CY, Dir]() { return OnReversePullArrow(CX, CY, Dir); });
	};

	// Movement arrows: the grid X axis is mirrored (Col = SizeX-1-X).
	// Visual RIGHT on screen = decreasing grid X = West direction.
	// Visual LEFT on screen  = increasing grid X = East direction.
	// Labels show visual arrows so the user knows which way things move on screen.
	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SButton)
			.ButtonColorAndOpacity(this, &SPuzzleEditorWidget::GetCellColor, X, Y)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ContentPadding(FMargin(0))
			.OnClicked_Lambda([this, CX, CY]() { return OnCellClicked(CX, CY); })
			[
				SNew(STextBlock)
				.Text(this, &SPuzzleEditorWidget::GetCellLabel, X, Y)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
				.Justification(ETextJustify::Center)
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top)
		[
			SNew(SBox)
			.WidthOverride(36.f).HeightOverride(20.f)
			[ ArrowBtn(FText::FromString(TEXT("^")), EPushBoxDirection::North) ]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		[
			SNew(SBox)
			.WidthOverride(36.f).HeightOverride(20.f)
			[ ArrowBtn(FText::FromString(TEXT("v")), EPushBoxDirection::South) ]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(36.f).HeightOverride(20.f)
			[ ArrowBtn(FText::FromString(TEXT(">")), EPushBoxDirection::West) ]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(36.f).HeightOverride(20.f)
			[ ArrowBtn(FText::FromString(TEXT("<")), EPushBoxDirection::East) ]
		];
}

FReply SPuzzleEditorWidget::OnReversePullArrow(int32 X, int32 Y, EPushBoxDirection Dir)
{
	UPuzzleEditorSubsystem* Sub = GetSubsystem();
	if (!Sub) return FReply::Handled();

	bool bSuccess = Sub->ReversePullBox(FIntPoint(X, Y), Dir);
	if (bSuccess)
	{
		RebuildGrid();
		StatusMessage = FText::FromString(FString::Printf(TEXT("Pulled box from (%d,%d) %s"),
			X, Y, *UPushBoxFunctionLibrary::DirectionToString(Dir)));
	}
	else
	{
		StatusMessage = FText::FromString(FString::Printf(TEXT("Cannot pull box at (%d,%d) %s"),
			X, Y, *UPushBoxFunctionLibrary::DirectionToString(Dir)));
	}

	return FReply::Handled();
}

TSharedRef<SWidget> SPuzzleEditorWidget::BuildReverseArrowsForPlayer(int32 X, int32 Y)
{
	int32 CX = X, CY = Y;

	auto ArrowBtn = [this, CX, CY](const FText& Label, EPushBoxDirection Dir)
	{
		return SNew(SButton)
			.Text(Label)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ButtonColorAndOpacity(FSlateColor(FLinearColor(0.2f, 0.5f, 0.8f, 0.9f)))
			.ContentPadding(FMargin(0))
			.OnClicked_Lambda([this, CX, CY, Dir]() { return OnReversePlayerMove(CX, CY, Dir); });
	};

	// Same mirror logic as box: visual RIGHT = West, visual LEFT = East
	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SButton)
			.ButtonColorAndOpacity(this, &SPuzzleEditorWidget::GetCellColor, X, Y)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ContentPadding(FMargin(0))
			.OnClicked_Lambda([this, CX, CY]() { return OnCellClicked(CX, CY); })
			[
				SNew(STextBlock)
				.Text(this, &SPuzzleEditorWidget::GetCellLabel, X, Y)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
				.Justification(ETextJustify::Center)
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top)
		[
			SNew(SBox)
			.WidthOverride(36.f).HeightOverride(20.f)
			[ ArrowBtn(FText::FromString(TEXT("^")), EPushBoxDirection::North) ]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		[
			SNew(SBox)
			.WidthOverride(36.f).HeightOverride(20.f)
			[ ArrowBtn(FText::FromString(TEXT("v")), EPushBoxDirection::South) ]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(36.f).HeightOverride(20.f)
			[ ArrowBtn(FText::FromString(TEXT(">")), EPushBoxDirection::West) ]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(36.f).HeightOverride(20.f)
			[ ArrowBtn(FText::FromString(TEXT("<")), EPushBoxDirection::East) ]
		];
}

FReply SPuzzleEditorWidget::OnReversePlayerMove(int32 X, int32 Y, EPushBoxDirection Dir)
{
	UPuzzleEditorSubsystem* Sub = GetSubsystem();
	if (!Sub) return FReply::Handled();

	bool bSuccess = Sub->ReversePlayerMove(Dir);
	if (bSuccess)
	{
		RebuildGrid();
		StatusMessage = FText::FromString(FString::Printf(TEXT("Moved player %s"),
			*UPushBoxFunctionLibrary::DirectionToString(Dir)));
	}
	else
	{
		StatusMessage = FText::FromString(FString::Printf(TEXT("Cannot move player %s (blocked)"),
			*UPushBoxFunctionLibrary::DirectionToString(Dir)));
	}

	return FReply::Handled();
}

TSharedRef<SWidget> SPuzzleEditorWidget::BuildDirectionArrowsForDoor(int32 X, int32 Y)
{
	int32 CX = X, CY = Y;

	auto DirBtn = [this, CX, CY](const FText& Label, EPushBoxDirection Dir)
	{
		return SNew(SButton)
			.Text(Label)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ButtonColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.55f, 0.15f, 0.9f)))
			.ContentPadding(FMargin(0))
			.OnClicked_Lambda([this, CX, CY, Dir]() { return OnDoorDirectionChanged(CX, CY, Dir); });
	};

	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SButton)
			.ButtonColorAndOpacity(this, &SPuzzleEditorWidget::GetCellColor, X, Y)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ContentPadding(FMargin(0))
			.OnClicked_Lambda([this, CX, CY]() { return OnCellClicked(CX, CY); })
			[
				SNew(STextBlock)
				.Text(this, &SPuzzleEditorWidget::GetCellLabel, X, Y)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
				.Justification(ETextJustify::Center)
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top)
		[
			SNew(SBox).WidthOverride(36.f).HeightOverride(20.f)
			[ DirBtn(FText::FromString(TEXT("N")), EPushBoxDirection::South) ]
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom)
		[
			SNew(SBox).WidthOverride(36.f).HeightOverride(20.f)
			[ DirBtn(FText::FromString(TEXT("S")), EPushBoxDirection::North) ]
		]
		// East on visual RIGHT
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(36.f).HeightOverride(20.f)
			[ DirBtn(FText::FromString(TEXT("E")), EPushBoxDirection::East) ]
		]
		// West on visual LEFT
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(36.f).HeightOverride(20.f)
			[ DirBtn(FText::FromString(TEXT("W")), EPushBoxDirection::West) ]
		];
}

FReply SPuzzleEditorWidget::OnDoorDirectionChanged(int32 X, int32 Y, EPushBoxDirection Dir)
{
	UPuzzleEditorSubsystem* Sub = GetSubsystem();
	if (!Sub) return FReply::Handled();

	Sub->SetCellOneWayDirection(FIntPoint(X, Y), Dir);
	RebuildGrid();

	// Dir is the stored (movement) direction; for N/S the user-facing entry label is opposite.
	FString DisplayDir;
	switch (Dir)
	{
	case EPushBoxDirection::North: DisplayDir = TEXT("South"); break;
	case EPushBoxDirection::South: DisplayDir = TEXT("North"); break;
	default: DisplayDir = UPushBoxFunctionLibrary::DirectionToString(Dir); break;
	}
	StatusMessage = FText::FromString(FString::Printf(TEXT("Door at (%d,%d) set to %s"),
		X, Y, *DisplayDir));
	return FReply::Handled();
}

// ------------------------------------------------------------------
// Toolbar
// ------------------------------------------------------------------

TSharedRef<SWidget> SPuzzleEditorWidget::BuildToolbar()
{
	auto ModeBtn = [this](const FText& Label, EPuzzleEditorMode Mode)
	{
		return SNew(SButton)
			.Text(Label)
			.ButtonColorAndOpacity(this, &SPuzzleEditorWidget::GetModeButtonColor, Mode)
			.OnClicked_Lambda([this, Mode]() { OnModeChanged(Mode); return FReply::Handled(); });
	};

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(2)
		[ ModeBtn(LOCTEXT("M1", "1: Tile Placement"), EPuzzleEditorMode::TilePlacement) ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2)
		[ ModeBtn(LOCTEXT("M2", "2: Target + Box"), EPuzzleEditorMode::GoalState) ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2)
		[ ModeBtn(LOCTEXT("M3", "3: Reverse (Rewind)"), EPuzzleEditorMode::ReverseEdit) ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 2, 0)
		[ SNew(SSeparator).Orientation(Orient_Vertical) ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2)
		[
			SNew(SButton)
			.Text(LOCTEXT("MCTSAutoGen", "Auto Generate (MCTS)"))
			.ToolTipText(LOCTEXT("MCTSAutoGenTooltip",
				"Fully automatic puzzle generation using Monte Carlo Tree Search.\n"
				"Generates room layout, box positions, player start, and obstacles.\n"
				"All generated puzzles are guaranteed to be solvable."))
			.OnClicked(this, &SPuzzleEditorWidget::OnMCTSAutoGenerate)
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2)
		[
			SNew(SButton)
			.Text(LOCTEXT("ClearGridBtn", "Clear Grid"))
			.ToolTipText(LOCTEXT("ClearGridTooltip",
				"Clear all cells to empty floor, keeping only the outer wall border.\n"
				"Removes all boxes, targets, move history, and obstacles."))
			.OnClicked(this, &SPuzzleEditorWidget::OnClearGrid)
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 2, 0)
		[ SNew(SSeparator).Orientation(Orient_Vertical) ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2)
		[
			SNew(SButton)
			.Text(LOCTEXT("SaveBtn", "Save"))
			.ToolTipText(LOCTEXT("SaveBtnTooltip",
				"Validate and save the puzzle.\n"
				"Choose to overwrite current asset or save as a new one."))
			.OnClicked(this, &SPuzzleEditorWidget::OnSave)
		];
}

// ------------------------------------------------------------------
// Properties Panel
// ------------------------------------------------------------------

TSharedRef<SWidget> SPuzzleEditorWidget::BuildPropertiesPanel()
{
	auto TileBtn = [this](const FText& Label, EPushBoxTileType Type)
	{
		return SNew(SButton).Text(Label)
			.ButtonColorAndOpacity(this, &SPuzzleEditorWidget::GetTileBrushButtonColor, Type)
			.OnClicked_Lambda([this, Type]() { SelectedTileBrush = Type; bEraserActive = false; return FReply::Handled(); });
	};

	auto DirBtn = [this](const FText& Label, EPushBoxDirection Dir)
	{
		return SNew(SButton).Text(Label)
			.ButtonColorAndOpacity(this, &SPuzzleEditorWidget::GetDirectionButtonColor, Dir)
			.OnClicked_Lambda([this, Dir]()
			{
				SelectedOneWayDirection = Dir;
				return FReply::Handled();
			});
	};

	return SNew(SScrollBox)
		+ SScrollBox::Slot().Padding(2)
		[
			SNew(SVerticalBox)

			// --- Load Existing Puzzle ---
			+ SVerticalBox::Slot().AutoHeight().Padding(2)
			[ SNew(STextBlock).Text(LOCTEXT("PDA", "Puzzle Data Asset:")) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(2)
			[
				SNew(SObjectPropertyEntryBox)
				.AllowedClass(UPuzzleLevelDataAsset::StaticClass())
				.ObjectPath(this, &SPuzzleEditorWidget::GetPuzzleDataAssetPath)
				.OnObjectChanged(this, &SPuzzleEditorWidget::OnPuzzleDataAssetChanged)
				.AllowClear(true)
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(2, 8, 2, 2)
			[ SNew(SSeparator) ]

			// --- WFC Asset ---
			+ SVerticalBox::Slot().AutoHeight().Padding(2, 8, 2, 2)
			[ SNew(STextBlock).Text(LOCTEXT("WA", "WFC Asset:")) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(2)
			[
				SNew(SObjectPropertyEntryBox)
				.AllowedClass(UWFCAsset::StaticClass())
				.ObjectPath(this, &SPuzzleEditorWidget::GetWFCAssetPath)
				.OnObjectChanged(this, &SPuzzleEditorWidget::OnWFCAssetChanged)
				.AllowClear(true)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(2)
			[
				SNew(STextBlock)
				.Text(this, &SPuzzleEditorWidget::GetWFCAssetInfoText)
				.Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
			]

			// --- Grid Size ---
			+ SVerticalBox::Slot().AutoHeight().Padding(2, 8, 2, 2)
			[ SNew(STextBlock).Text(LOCTEXT("GS", "Grid Size:")) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SSpinBox<int32>).MinValue(3).MaxValue(32)
					.Value_Lambda([this]() { return PendingGridSize.X; })
					.OnValueChanged_Lambda([this](int32 V) { PendingGridSize.X = V; })
					.OnValueCommitted_Lambda([this](int32 V, ETextCommit::Type)
					{
						PendingGridSize.X = V;
						if (UPuzzleEditorSubsystem* S = GetSubsystem()) S->SyncWFCGridSize(PendingGridSize);
					})
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0)
				[ SNew(STextBlock).Text(LOCTEXT("x", "x")) ]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SSpinBox<int32>).MinValue(3).MaxValue(32)
					.Value_Lambda([this]() { return PendingGridSize.Y; })
					.OnValueChanged_Lambda([this](int32 V) { PendingGridSize.Y = V; })
					.OnValueCommitted_Lambda([this](int32 V, ETextCommit::Type)
					{
						PendingGridSize.Y = V;
						if (UPuzzleEditorSubsystem* S = GetSubsystem()) S->SyncWFCGridSize(PendingGridSize);
					})
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("RefreshGrid", "Apply"))
					.ToolTipText(LOCTEXT("RefreshGridTooltip",
						"Apply the grid size.\n"
						"If a puzzle DataAsset is loaded with matching size, its content is reloaded.\n"
						"Otherwise, the grid is resized and cleared."))
					.OnClicked_Lambda([this]()
					{
						if (UPuzzleEditorSubsystem* S = GetSubsystem())
						{
							UPuzzleLevelDataAsset* PD = S->GetActivePuzzleData();
							if (PD && PD->GridSize == PendingGridSize)
							{
								// Reload the DataAsset content instead of clearing
								S->LoadPuzzleData(PD);
								RebuildGrid();
								StatusMessage = FText::Format(
									LOCTEXT("GridReloaded", "Reloaded puzzle data ({0}x{1})."),
									FText::AsNumber(PendingGridSize.X),
									FText::AsNumber(PendingGridSize.Y));
							}
							else
							{
								S->ResizeGrid(PendingGridSize);
								RebuildGrid();
								StatusMessage = FText::Format(
									LOCTEXT("GridResized", "Grid resized to {0}x{1}."),
									FText::AsNumber(PendingGridSize.X),
									FText::AsNumber(PendingGridSize.Y));
							}
						}
						return FReply::Handled();
					})
				]
			]

			// --- Current Mode ---
			+ SVerticalBox::Slot().AutoHeight().Padding(2, 8, 2, 2)
			[
				SNew(STextBlock)
				.Text(this, &SPuzzleEditorWidget::GetModeText)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
			]

			// --- Move Count ---
			+ SVerticalBox::Slot().AutoHeight().Padding(2)
			[ SNew(STextBlock).Text(this, &SPuzzleEditorWidget::GetMoveCountText) ]

			// --- Undo ---
			+ SVerticalBox::Slot().AutoHeight().Padding(2, 8, 2, 2)
			[
				SNew(SButton).Text(LOCTEXT("Undo", "Undo"))
				.OnClicked_Lambda([this]()
				{
					if (UPuzzleEditorSubsystem* S = GetSubsystem()) S->UndoReversePull();
					return FReply::Handled();
				})
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(2, 12, 2, 2)
			[ SNew(SSeparator) ]

			// --- Tile Brush ---
			+ SVerticalBox::Slot().AutoHeight().Padding(2, 4, 2, 2)
			[ SNew(STextBlock).Text(LOCTEXT("TB", "Tile Brush:")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 10)) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(2)
			[
				SNew(SWrapBox).UseAllottedSize(true)
				+ SWrapBox::Slot().Padding(1) [ TileBtn(LOCTEXT("F", "Floor"), EPushBoxTileType::Empty) ]
				+ SWrapBox::Slot().Padding(1) [ TileBtn(LOCTEXT("W", "Wall"), EPushBoxTileType::Wall) ]
				+ SWrapBox::Slot().Padding(1) [ TileBtn(LOCTEXT("I", "Ice"), EPushBoxTileType::Ice) ]
				+ SWrapBox::Slot().Padding(1) [ TileBtn(LOCTEXT("OWD", "OneWayDoor"), EPushBoxTileType::OneWayDoor) ]
				+ SWrapBox::Slot().Padding(1) [ TileBtn(LOCTEXT("PG", "PlayerGoal"), EPushBoxTileType::PlayerGoal) ]
				+ SWrapBox::Slot().Padding(1) [ TileBtn(LOCTEXT("TM", "Target"), EPushBoxTileType::TargetMarker) ]
			]

			// --- Eraser ---
			+ SVerticalBox::Slot().AutoHeight().Padding(2, 4, 2, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("Eraser", "Eraser"))
				.ToolTipText(LOCTEXT("EraserTooltip",
					"Toggle eraser mode. When active, click or drag on any cell\n"
					"to reset it to empty floor (removes walls, ice, doors, boxes, etc.)."))
				.ButtonColorAndOpacity(this, &SPuzzleEditorWidget::GetEraserButtonColor)
				.OnClicked_Lambda([this]()
				{
					bEraserActive = !bEraserActive;
					if (bEraserActive)
						StatusMessage = LOCTEXT("EraserOn", "Eraser active ！ click/drag to erase tiles.");
					else
						StatusMessage = LOCTEXT("EraserOff", "Eraser deactivated.");
					return FReply::Handled();
				})
			]

			// --- OneWayDoor Direction ---
			+ SVerticalBox::Slot().AutoHeight().Padding(2, 8, 2, 2)
			[ SNew(STextBlock).Text(LOCTEXT("DD", "Door Direction:")) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(1) [ DirBtn(LOCTEXT("DN", "N"), EPushBoxDirection::North) ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(1) [ DirBtn(LOCTEXT("DS", "S"), EPushBoxDirection::South) ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(1) [ DirBtn(LOCTEXT("DE", "E"), EPushBoxDirection::East) ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(1) [ DirBtn(LOCTEXT("DW", "W"), EPushBoxDirection::West) ]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(2, 12, 2, 2)
			[ SNew(SSeparator) ]

			// --- MCTS Auto-Generation Config ---
			+ SVerticalBox::Slot().AutoHeight().Padding(2, 4, 2, 2)
			[ SNew(STextBlock).Text(LOCTEXT("MCTSHeader", "MCTS Auto-Generate:")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 10)) ]

			+ SVerticalBox::Slot().AutoHeight().Padding(2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(1)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight() [ SNew(STextBlock).Text(LOCTEXT("MCTSBoxes", "Boxes:")) ]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SSpinBox<int32>).MinValue(1).MaxValue(8)
						.Value(MCTSConfig.NumBoxes)
						.OnValueCommitted_Lambda([this](int32 V, ETextCommit::Type) { MCTSConfig.NumBoxes = V; })
						.OnValueChanged_Lambda([this](int32 V) { MCTSConfig.NumBoxes = V; })
					]
				]
				+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(1)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight() [ SNew(STextBlock).Text(LOCTEXT("MCTSTime", "Time (s):")) ]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SSpinBox<float>).MinValue(1.f).MaxValue(120.f)
						.Value(MCTSConfig.TimeBudgetSeconds)
						.OnValueCommitted_Lambda([this](float V, ETextCommit::Type) { MCTSConfig.TimeBudgetSeconds = V; })
						.OnValueChanged_Lambda([this](float V) { MCTSConfig.TimeBudgetSeconds = V; })
					]
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(1)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight() [ SNew(STextBlock).Text(LOCTEXT("MCTSMinPulls", "Min Pulls:")) ]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SSpinBox<int32>).MinValue(2).MaxValue(50)
						.Value(MCTSConfig.MinReversePulls)
						.OnValueCommitted_Lambda([this](int32 V, ETextCommit::Type) { MCTSConfig.MinReversePulls = V; })
						.OnValueChanged_Lambda([this](int32 V) { MCTSConfig.MinReversePulls = V; })
					]
				]
				+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(1)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight() [ SNew(STextBlock).Text(LOCTEXT("MCTSMaxPulls", "Max Pulls:")) ]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SSpinBox<int32>).MinValue(5).MaxValue(100)
						.Value(MCTSConfig.MaxReversePulls)
						.OnValueCommitted_Lambda([this](int32 V, ETextCommit::Type) { MCTSConfig.MaxReversePulls = V; })
						.OnValueChanged_Lambda([this](int32 V) { MCTSConfig.MaxReversePulls = V; })
					]
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(1)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight() [ SNew(STextBlock).Text(LOCTEXT("MCTSMinDisp", "Min Displacement:")) ]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SSpinBox<int32>).MinValue(2).MaxValue(10)
						.Value(MCTSConfig.MinBoxDisplacement)
						.OnValueCommitted_Lambda([this](int32 V, ETextCommit::Type) { MCTSConfig.MinBoxDisplacement = V; })
						.OnValueChanged_Lambda([this](int32 V) { MCTSConfig.MinBoxDisplacement = V; })
					]
				]
				+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(1)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight() [ SNew(STextBlock).Text(LOCTEXT("MCTSSeed", "Seed (0=rand):")) ]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SSpinBox<int32>).MinValue(0).MaxValue(999999)
						.Value(MCTSConfig.Seed)
						.OnValueCommitted_Lambda([this](int32 V, ETextCommit::Type) { MCTSConfig.Seed = V; })
						.OnValueChanged_Lambda([this](int32 V) { MCTSConfig.Seed = V; })
					]
				]
			]

			// --- Room & Obstacles ---
			+ SVerticalBox::Slot().AutoHeight().Padding(2, 8, 2, 2)
			[ SNew(STextBlock).Text(LOCTEXT("MCTSObstHeader", "Room & Obstacles:")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 9)) ]

			+ SVerticalBox::Slot().AutoHeight().Padding(2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(1)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight() [ SNew(STextBlock).Text(LOCTEXT("MCTSFloor", "Floor Ratio:")) ]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SSpinBox<float>).MinValue(0.5f).MaxValue(1.f).Delta(0.05f)
						.Value(MCTSConfig.FloorRatio)
						.OnValueCommitted_Lambda([this](float V, ETextCommit::Type) { MCTSConfig.FloorRatio = V; })
						.OnValueChanged_Lambda([this](float V) { MCTSConfig.FloorRatio = V; })
					]
				]
				+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(1)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight() [ SNew(STextBlock).Text(LOCTEXT("MCTSWall", "Wall Density:")) ]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SSpinBox<float>).MinValue(0.f).MaxValue(0.5f).Delta(0.05f)
						.Value(MCTSConfig.WallDensity)
						.OnValueCommitted_Lambda([this](float V, ETextCommit::Type) { MCTSConfig.WallDensity = V; })
						.OnValueChanged_Lambda([this](float V) { MCTSConfig.WallDensity = V; })
					]
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(1)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight() [ SNew(STextBlock).Text(LOCTEXT("MCTSIce", "Ice Density:")) ]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SSpinBox<float>).MinValue(0.f).MaxValue(0.5f).Delta(0.05f)
						.Value(MCTSConfig.IceDensity)
						.OnValueCommitted_Lambda([this](float V, ETextCommit::Type) { MCTSConfig.IceDensity = V; })
						.OnValueChanged_Lambda([this](float V) { MCTSConfig.IceDensity = V; })
					]
				]
				+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(1)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight() [ SNew(STextBlock).Text(LOCTEXT("MCTSDoor", "Door Density:")) ]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SSpinBox<float>).MinValue(0.f).MaxValue(0.5f).Delta(0.05f)
						.Value(MCTSConfig.OneWayDoorDensity)
						.OnValueCommitted_Lambda([this](float V, ETextCommit::Type) { MCTSConfig.OneWayDoorDensity = V; })
						.OnValueChanged_Lambda([this](float V) { MCTSConfig.OneWayDoorDensity = V; })
					]
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(2, 12, 2, 2)
			[ SNew(SSeparator) ]

			// --- Reverse Mode Info ---
			+ SVerticalBox::Slot().AutoHeight().Padding(2, 4, 2, 2)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ReverseInfo", "Reverse Edit:\nClick arrows on box cells\nto pull them."))
				.Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
				.AutoWrapText(true)
			]
		];
}

TSharedRef<SWidget> SPuzzleEditorWidget::BuildStatusBar()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f)
		[ SNew(STextBlock).Text(this, &SPuzzleEditorWidget::GetStatusText) ]
		+ SHorizontalBox::Slot().AutoWidth()
		[ SNew(STextBlock).Text(this, &SPuzzleEditorWidget::GetGridSizeText) ];
}

// ------------------------------------------------------------------
// Cell rendering
// ------------------------------------------------------------------

FSlateColor SPuzzleEditorWidget::GetCellColor(int32 X, int32 Y) const
{
	UPuzzleEditorSubsystem* Sub = GetSubsystem();
	if (!Sub) return FSlateColor(FLinearColor::Gray);
	UPushBoxGridManager* GM = Sub->GetActiveGridManager();
	if (!GM) return FSlateColor(FLinearColor::Gray);

	FIntPoint Coord(X, Y);

	if (GM->PlayerGridPosition == Coord)
		return FSlateColor(FLinearColor(0.2f, 0.4f, 0.9f));

	if (GM->HasBoxAt(Coord))
	{
		FPushBoxCellData Cell = GM->GetCellData(Coord);
		if (Cell.IsTarget())
			return FSlateColor(FLinearColor(0.1f, 0.9f, 0.1f));
		return FSlateColor(FLinearColor(0.8f, 0.5f, 0.1f));
	}

	FPushBoxCellData Cell = GM->GetCellData(Coord);

	switch (Cell.TileType)
	{
	case EPushBoxTileType::Wall:         return FSlateColor(FLinearColor(0.15f, 0.15f, 0.15f));
	case EPushBoxTileType::Ice:          return FSlateColor(FLinearColor(0.6f, 0.9f, 1.0f));
	case EPushBoxTileType::OneWayDoor:   return FSlateColor(FLinearColor(0.9f, 0.7f, 0.2f));
	case EPushBoxTileType::PlayerGoal:   return FSlateColor(FLinearColor(1.0f, 0.85f, 0.0f));
	case EPushBoxTileType::TargetMarker: return FSlateColor(FLinearColor(0.4f, 0.9f, 0.4f, 0.5f));
	default:                             return FSlateColor(FLinearColor(0.35f, 0.35f, 0.35f));
	}
}

FText SPuzzleEditorWidget::GetCellLabel(int32 X, int32 Y) const
{
	UPuzzleEditorSubsystem* Sub = GetSubsystem();
	if (!Sub) return FText::GetEmpty();
	UPushBoxGridManager* GM = Sub->GetActiveGridManager();
	if (!GM) return FText::GetEmpty();

	FIntPoint Coord(X, Y);
	FString Label;

	if (GM->PlayerGridPosition == Coord) Label = TEXT("Player");

	if (GM->HasBoxAt(Coord))
	{
		FPushBoxCellData Cell = GM->GetCellData(Coord);
		return FText::FromString(Cell.IsTarget() ? TEXT("Box\n+Target") : TEXT("Box"));
	}

	FPushBoxCellData Cell = GM->GetCellData(Coord);

	if (!Label.IsEmpty() && Cell.TileType != EPushBoxTileType::Empty)
		Label += TEXT("\n");

	switch (Cell.TileType)
	{
	case EPushBoxTileType::Wall:   Label += TEXT("Wall"); break;
	case EPushBoxTileType::Ice:    Label += TEXT("Ice"); break;
	case EPushBoxTileType::OneWayDoor:
	{
		// OneWayDirection stores box movement direction, but the label should
		// show the entry side (user-facing). N/S are swapped in storage,
		// so swap them back for display.
		FString D;
		switch (Cell.OneWayDirection)
		{
		case EPushBoxDirection::North: D = TEXT("S"); break;
		case EPushBoxDirection::South: D = TEXT("N"); break;
		case EPushBoxDirection::East:  D = TEXT("E"); break;
		case EPushBoxDirection::West:  D = TEXT("W"); break;
		default: D = TEXT("?"); break;
		}
		Label += FString::Printf(TEXT("Door(%s)"), *D);
		break;
	}
	case EPushBoxTileType::PlayerGoal: Label += TEXT("Goal"); break;
	case EPushBoxTileType::TargetMarker: Label += TEXT("Target"); break;
	default: break;
	}

	return FText::FromString(Label);
}

void SPuzzleEditorWidget::ApplyBrushToCell(int32 X, int32 Y)
{
	UPuzzleEditorSubsystem* Sub = GetSubsystem();
	if (!Sub) return;
	UPushBoxGridManager* GM = Sub->GetActiveGridManager();
	if (!GM) return;

	if (Sub->GetEditMode() != EPuzzleEditorMode::TilePlacement)
		return;

	FIntPoint Coord(X, Y);

	if (bEraserActive)
	{
		// Eraser: reset any cell to Empty floor, remove box if present
		if (GM->HasBoxAt(Coord))
			Sub->RemoveBox(Coord);
		Sub->SetCellType(Coord, EPushBoxTileType::Empty);
	}
	else
	{
		FPushBoxCellData Cell = GM->GetCellData(Coord);
		// Always paint (no toggle) ！ used for both click and drag.
		// Toggle behavior is handled in OnCellClicked separately.
		if (Cell.TileType != SelectedTileBrush)
		{
			Sub->SetCellType(Coord, SelectedTileBrush);
			if (SelectedTileBrush == EPushBoxTileType::OneWayDoor)
				Sub->SetCellOneWayDirection(Coord, SelectedOneWayDirection);
		}
	}
}

FReply SPuzzleEditorWidget::OnCellClicked(int32 X, int32 Y)
{
	UPuzzleEditorSubsystem* Sub = GetSubsystem();
	if (!Sub) return FReply::Handled();

	FIntPoint Coord(X, Y);

	switch (Sub->GetEditMode())
	{
	case EPuzzleEditorMode::TilePlacement:
	{
		if (bEraserActive)
		{
			ApplyBrushToCell(X, Y);
		}
		else
		{
			UPushBoxGridManager* GM = Sub->GetActiveGridManager();
			if (GM)
			{
				FPushBoxCellData Cell = GM->GetCellData(Coord);
				if (Cell.TileType == SelectedTileBrush)
				{
					// Toggle: clicking same tile type ★ revert to Empty
					Sub->SetCellType(Coord, EPushBoxTileType::Empty);
				}
				else
				{
					Sub->SetCellType(Coord, SelectedTileBrush);
					if (SelectedTileBrush == EPushBoxTileType::OneWayDoor)
						Sub->SetCellOneWayDirection(Coord, SelectedOneWayDirection);
				}
			}
		}
		RebuildGrid();
		break;
	}

	case EPuzzleEditorMode::GoalState:
		if (UPushBoxGridManager* GM = Sub->GetActiveGridManager())
		{
			FPushBoxCellData Cell = GM->GetCellData(Coord);
			if (Cell.TileType == EPushBoxTileType::TargetMarker)
			{
				// TargetMarker already exists ！ clicking it removes the whole tile (reverts to Floor)
				Sub->SetCellType(Coord, EPushBoxTileType::Empty);
			}
			else if (GM->HasBoxAt(Coord))
			{
				Sub->RemoveBox(Coord);
			}
			else
			{
				// Place a TargetMarker (which auto-creates box + target)
				Sub->SetCellType(Coord, EPushBoxTileType::TargetMarker);
			}
		}
		break;

	case EPuzzleEditorMode::ReverseEdit:
		// Clicking non-arrow cells in reverse mode does nothing
		// (arrows on box/player cells handle reverse pulls/moves)
		break;

	default:
		break;
	}

	return FReply::Handled();
}

// ------------------------------------------------------------------
// Button highlight helpers
// ------------------------------------------------------------------

FSlateColor SPuzzleEditorWidget::GetModeButtonColor(EPuzzleEditorMode Mode) const
{
	if (UPuzzleEditorSubsystem* Sub = GetSubsystem())
	{
		if (Sub->GetEditMode() == Mode)
			return FSlateColor(FLinearColor(0.1f, 0.6f, 1.0f));
	}
	return FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f));
}

FSlateColor SPuzzleEditorWidget::GetTileBrushButtonColor(EPushBoxTileType Type) const
{
	if (!bEraserActive && SelectedTileBrush == Type)
		return FSlateColor(FLinearColor(0.1f, 0.6f, 1.0f));
	return FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f));
}

FSlateColor SPuzzleEditorWidget::GetEraserButtonColor() const
{
	if (bEraserActive)
		return FSlateColor(FLinearColor(1.0f, 0.3f, 0.3f)); // red highlight
	return FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f));
}

FSlateColor SPuzzleEditorWidget::GetDirectionButtonColor(EPushBoxDirection Dir) const
{
	EPushBoxDirection Active = SelectedOneWayDirection;
	if (Active == Dir)
		return FSlateColor(FLinearColor(0.1f, 0.6f, 1.0f));
	return FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f));
}

// ------------------------------------------------------------------
// Toolbar actions
// ------------------------------------------------------------------

FReply SPuzzleEditorWidget::OnMCTSAutoGenerate()
{
	UPuzzleEditorSubsystem* Sub = GetSubsystem();
	if (!Sub) return FReply::Handled();

	// Sync MCTS grid size with PendingGridSize
	MCTSConfig.GridSize = PendingGridSize;

	// Ensure there is an active puzzle data asset
	if (!Sub->GetActivePuzzleData())
	{
		FText Msg = LOCTEXT("MCTSNeedAsset",
			"No active puzzle data asset.\n"
			"Please create or select a puzzle first (via the asset picker above).");
		FMessageDialog::Open(EAppMsgType::Ok, Msg);
		return FReply::Handled();
	}

	// Show progress dialog
	{
		FScopedSlowTask SlowTask(100.f,
			FText::Format(LOCTEXT("MCTSProgress",
				"Generating puzzle with MCTS...\n"
				"Grid: {0}x{1}, Boxes: {2}\n"
				"(Auto-retries with relaxed constraints if needed)"),
				FText::AsNumber(MCTSConfig.GridSize.X),
				FText::AsNumber(MCTSConfig.GridSize.Y),
				FText::AsNumber(MCTSConfig.NumBoxes)));
		SlowTask.MakeDialog(false);

		SlowTask.EnterProgressFrame(5.f, LOCTEXT("MCTSSearching", "Running MCTS search (with adaptive retry)..."));
		FSlateApplication::Get().PumpMessages();
		FSlateApplication::Get().Tick();

		FMCTSResult Result = Sub->MCTSAutoGenerate(MCTSConfig);

		SlowTask.EnterProgressFrame(90.f, LOCTEXT("MCTSApplying", "Applying result..."));

		if (Result.bSuccess)
		{
			PendingGridSize = Result.StartState.GridSize;
			RebuildGrid();
			StatusMessage = FText::Format(
				LOCTEXT("MCTSDone",
					"MCTS: Score={0}, Moves={1}, Sims={2}, Time={3}s"),
				FText::AsNumber(static_cast<int32>(Result.BestScore * 1000) / 1000.f),
				FText::AsNumber(Result.ForwardSolution.Num()),
				FText::AsNumber(Result.SimulationCount),
				FText::AsNumber(static_cast<int32>(Result.ElapsedSeconds * 10) / 10.f));
		}
		else
		{
			StatusMessage = FText::Format(
				LOCTEXT("MCTSFailed",
					"MCTS failed after all retries ({0} sims in {1}s).\n"
					"The grid may be too small for {2} boxes, or constraints are too strict."),
				FText::AsNumber(Result.SimulationCount),
				FText::AsNumber(static_cast<int32>(Result.ElapsedSeconds * 10) / 10.f),
				FText::AsNumber(MCTSConfig.NumBoxes));

			FMessageDialog::Open(EAppMsgType::Ok, StatusMessage);
		}

		SlowTask.EnterProgressFrame(5.f, LOCTEXT("MCTSComplete", "Done."));
	}

	return FReply::Handled();
}

FReply SPuzzleEditorWidget::OnClearGrid()
{
	if (UPuzzleEditorSubsystem* Sub = GetSubsystem())
	{
		UPushBoxGridManager* GM = Sub->GetActiveGridManager();
		if (GM)
		{
			// Clear all cells to empty floor, keeping only the outer wall border
			// (removes all boxes, targets, move history, and obstacles)
			Sub->ClearGrid();
			PendingGridSize = GM->GridSize;
			RebuildGrid();
			StatusMessage = LOCTEXT("GridCleared", "Grid cleared. All cells reset to empty.");
		}
		else
		{
			StatusMessage = LOCTEXT("NoGM", "No active grid. Select a puzzle or create one via the asset picker.");
		}
	}
	return FReply::Handled();
}

FReply SPuzzleEditorWidget::OnSave()
{
	UPuzzleEditorSubsystem* Sub = GetSubsystem();
	if (!Sub)
		return FReply::Handled();

	UPuzzleLevelDataAsset* CurrentAsset = Sub->GetActivePuzzleData();

	// --- Decide save target ---
	// 0 = save to current, 1 = save as new, -1 = cancelled
	int32 SaveChoice = -1;

	if (CurrentAsset)
	{
		// Asset exists ！ ask: overwrite or save as new?
		EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNoCancel,
			FText::Format(
				LOCTEXT("SaveChoice",
					"Current puzzle: {0}\n\n"
					"[Yes] ！ Overwrite this asset\n"
					"[No] ！ Save as a new asset\n"
					"[Cancel] ！ Cancel"),
				FText::FromString(CurrentAsset->GetName())));

		if (Result == EAppReturnType::Yes)
			SaveChoice = 0;
		else if (Result == EAppReturnType::No)
			SaveChoice = 1;
		// Cancel ★ SaveChoice stays -1
	}
	else
	{
		// No current asset ！ go straight to "save as new"
		SaveChoice = 1;
	}

	if (SaveChoice == -1)
	{
		StatusMessage = LOCTEXT("SaveCancelled", "Save cancelled.");
		return FReply::Handled();
	}

	// --- Save as new: ask for a name ---
	if (SaveChoice == 1)
	{
		// Build a small input dialog using a modal window
		FString DefaultName = CurrentAsset ? CurrentAsset->GetName() + TEXT("_Copy") : TEXT("NewPuzzle");
		bool bNameCancelled = false;

		TSharedRef<SEditableTextBox> NameBox = SNew(SEditableTextBox)
			.Text(FText::FromString(DefaultName));

		TSharedRef<SWindow> NameWindow = SNew(SWindow)
			.Title(LOCTEXT("SaveAsTitle", "Save As New Puzzle"))
			.SizingRule(ESizingRule::Autosized)
			.SupportsMaximize(false)
			.SupportsMinimize(false)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(8)
				[ SNew(STextBlock).Text(LOCTEXT("EnterName", "Enter puzzle name:")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(8, 2)
				[ SNew(SBox).MinDesiredWidth(300.f) [ NameBox ] ]
				+ SVerticalBox::Slot().AutoHeight().Padding(8, 8)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f)
					+ SHorizontalBox::Slot().AutoWidth().Padding(2)
					[
						SNew(SButton).Text(LOCTEXT("OK", "OK"))
						.OnClicked_Lambda([&NameWindow]()
						{
							NameWindow->RequestDestroyWindow();
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2)
					[
						SNew(SButton).Text(LOCTEXT("Cancel", "Cancel"))
						.OnClicked_Lambda([&NameWindow, &bNameCancelled]()
						{
							bNameCancelled = true;
							NameWindow->RequestDestroyWindow();
							return FReply::Handled();
						})
					]
				]
			];

		// Show as modal ！ blocks until window is closed
		FSlateApplication::Get().AddModalWindow(NameWindow, FSlateApplication::Get().GetActiveTopLevelWindow());

		if (bNameCancelled)
		{
			StatusMessage = LOCTEXT("SaveCancelled", "Save cancelled.");
			return FReply::Handled();
		}

		FString NewName = NameBox->GetText().ToString().TrimStartAndEnd();
		if (NewName.IsEmpty())
		{
			StatusMessage = LOCTEXT("SaveEmptyName", "Save cancelled ！ name cannot be empty.");
			return FReply::Handled();
		}

		// Create the new asset ！ preserve the current editing state
		UPushBoxGridManager* GM = Sub->GetActiveGridManager();
		FIntPoint GridSize = GM ? GM->GridSize : PendingGridSize;

		// Capture current state before CreateNewPuzzle resets everything
		FPushBoxGridSnapshot SavedGoalSnapshot = Sub->GetGoalSnapshot();
		TArray<FPushBoxCellData> SavedCells;
		TArray<FIntPoint> SavedBoxPositions;
		FIntPoint SavedPlayerPos = FIntPoint::ZeroValue;
		TArray<FPushBoxMoveRecord> SavedMoveHistory;
		int32 SavedMoveCount = 0;
		EPuzzleState SavedPuzzleState = EPuzzleState::NotStarted;
		if (GM)
		{
			SavedCells = GM->Cells;
			SavedBoxPositions = GM->BoxPositions;
			SavedPlayerPos = GM->PlayerGridPosition;
			SavedMoveHistory = GM->MoveHistory;
			SavedMoveCount = GM->MoveCount;
			SavedPuzzleState = GM->CurrentPuzzleState;
		}

		Sub->CreateNewPuzzle(NewName, GridSize);

		// Restore the saved state into the new grid manager
		if (UPushBoxGridManager* NewGM = Sub->GetActiveGridManager())
		{
			NewGM->Cells = SavedCells;
			NewGM->BoxPositions = SavedBoxPositions;
			NewGM->PlayerGridPosition = SavedPlayerPos;
			NewGM->MoveHistory = SavedMoveHistory;
			NewGM->MoveCount = SavedMoveCount;
			NewGM->CurrentPuzzleState = SavedPuzzleState;
		}

		// Restore goal snapshot (CreateNewPuzzle resets it)
		Sub->SetGoalSnapshot(SavedGoalSnapshot);

		RebuildGrid();
	}

	// --- Validate and save ---
	FText SaveError;
	if (Sub->ValidateAndSave(SaveError))
	{
		StatusMessage = FText::Format(
			LOCTEXT("SavedAs", "Saved: {0}"),
			FText::FromString(Sub->GetActivePuzzleData()->GetName()));
	}
	else
	{
		if (SaveError.IsEmpty())
			StatusMessage = LOCTEXT("SaveCancelled2", "Save cancelled.");
		else
			StatusMessage = FText::Format(LOCTEXT("SaveErr", "Save failed: {0}"), SaveError);
	}
	RebuildGrid();

	return FReply::Handled();
}

void SPuzzleEditorWidget::OnModeChanged(EPuzzleEditorMode NewMode)
{
	if (UPuzzleEditorSubsystem* Sub = GetSubsystem())
	{
		Sub->SetEditMode(NewMode);
		// Rebuild grid so mode-specific UI (e.g., reverse arrows) updates
		RebuildGrid();
	}
}

// ------------------------------------------------------------------
// Text getters
// ------------------------------------------------------------------

FText SPuzzleEditorWidget::GetGridSizeText() const
{
	UPuzzleEditorSubsystem* Sub = GetSubsystem();
	UPushBoxGridManager* GM = Sub ? Sub->GetActiveGridManager() : nullptr;
	if (GM && GM->GridSize.X > 0)
		return FText::FromString(FString::Printf(TEXT("Grid: %dx%d"), GM->GridSize.X, GM->GridSize.Y));
	return FText::FromString(FString::Printf(TEXT("Grid: %dx%d"), PendingGridSize.X, PendingGridSize.Y));
}

FText SPuzzleEditorWidget::GetMoveCountText() const
{
	UPuzzleEditorSubsystem* Sub = GetSubsystem();
	UPushBoxGridManager* GM = Sub ? Sub->GetActiveGridManager() : nullptr;
	if (GM) return FText::FromString(FString::Printf(TEXT("Moves: %d"), GM->MoveCount));
	return FText::FromString(TEXT("Moves: 0"));
}

FText SPuzzleEditorWidget::GetStatusText() const { return StatusMessage; }

FText SPuzzleEditorWidget::GetModeText() const
{
	UPuzzleEditorSubsystem* Sub = GetSubsystem();
	if (!Sub) return LOCTEXT("NoMode", "No Active Puzzle");
	switch (Sub->GetEditMode())
	{
	case EPuzzleEditorMode::TilePlacement: return LOCTEXT("MT", "Mode: Tile Placement");
	case EPuzzleEditorMode::GoalState:     return LOCTEXT("MG", "Mode: Goal State");
	case EPuzzleEditorMode::ReverseEdit:   return LOCTEXT("MR", "Mode: Reverse Edit");
	default:                               return LOCTEXT("MU", "Mode: Unknown");
	}
}

UPuzzleEditorSubsystem* SPuzzleEditorWidget::GetSubsystem() const
{
	return GEditor ? GEditor->GetEditorSubsystem<UPuzzleEditorSubsystem>() : nullptr;
}

// ------------------------------------------------------------------
// WFC Asset picker helpers
// ------------------------------------------------------------------

FString SPuzzleEditorWidget::GetPuzzleDataAssetPath() const
{
	UPuzzleEditorSubsystem* Sub = GetSubsystem();
	if (!Sub) return FString();
	UPuzzleLevelDataAsset* PD = Sub->GetActivePuzzleData();
	if (!PD) return FString();
	return PD->GetPathName();
}

void SPuzzleEditorWidget::OnPuzzleDataAssetChanged(const FAssetData& AssetData)
{
	UPuzzleEditorSubsystem* Sub = GetSubsystem();
	if (!Sub) return;

	UPuzzleLevelDataAsset* PD = Cast<UPuzzleLevelDataAsset>(AssetData.GetAsset());
	if (PD)
	{
		Sub->LoadPuzzleData(PD);
		PendingGridSize = PD->GridSize;
		RebuildGrid();
		StatusMessage = FText::FromString(FString::Printf(TEXT("Loaded puzzle: %s (%dx%d)"),
			*PD->PuzzleName.ToString(), PD->GridSize.X, PD->GridSize.Y));
	}
	else
	{
		StatusMessage = FText::FromString(TEXT("Puzzle data asset cleared."));
	}
}

FString SPuzzleEditorWidget::GetWFCAssetPath() const
{
	UPuzzleEditorSubsystem* Sub = GetSubsystem();
	if (!Sub) return FString();
	UPuzzleLevelDataAsset* PD = Sub->GetActivePuzzleData();
	if (!PD) return FString();
	return PD->WFCAssetRef.ToString();
}

void SPuzzleEditorWidget::OnWFCAssetChanged(const FAssetData& AssetData)
{
	UPuzzleEditorSubsystem* Sub = GetSubsystem();
	if (!Sub) return;

	UWFCAsset* WFCAsset = Cast<UWFCAsset>(AssetData.GetAsset());
	Sub->SetWFCAsset(WFCAsset);

	if (WFCAsset)
	{
		StatusMessage = FText::FromString(FString::Printf(TEXT("WFC asset set: %s"), *WFCAsset->GetName()));
	}
	else
	{
		StatusMessage = FText::FromString(TEXT("WFC asset cleared."));
	}
}

FText SPuzzleEditorWidget::GetWFCAssetInfoText() const
{
	UPuzzleEditorSubsystem* Sub = GetSubsystem();
	if (!Sub) return FText::GetEmpty();
	UPuzzleLevelDataAsset* PD = Sub->GetActivePuzzleData();
	if (!PD) return FText::FromString(TEXT("No active puzzle."));

	UWFCAsset* WFCAsset = PD->WFCAssetRef.LoadSynchronous();
	if (!WFCAsset)
		return FText::FromString(TEXT("No WFC asset assigned. Grid size changes will not sync."));

	if (!WFCAsset->GridConfig)
		return FText::FromString(TEXT("WFC asset has no GridConfig."));

	if (UWFCGrid2DConfig* Cfg = Cast<UWFCGrid2DConfig>(WFCAsset->GridConfig))
	{
		return FText::FromString(FString::Printf(
			TEXT("WFC 2D Grid: %dx%d  (will sync with Grid Size)"),
			Cfg->Dimensions.X, Cfg->Dimensions.Y));
	}

	return FText::FromString(TEXT("WFC asset: unknown grid config type."));
}

#undef LOCTEXT_NAMESPACE
