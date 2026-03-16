// Copyright PushBox Game. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "PushBoxTypes.generated.h"

/** Log category for PushBox game */
DECLARE_LOG_CATEGORY_EXTERN(LogPushBox, Log, All);

// ============================================================================
// Grid & Tile Enums
// ============================================================================

/**
 * The type of tile occupying a grid cell.
 * 
 * Movement rules (defined in UPushBoxGridManager):
 *   Player: can walk on any tile EXCEPT Wall. Blocked by boxes.
 *   Box:    can be pushed onto Floor/Target/Ice/OneWayDoor(if direction matches).
 *           Blocked by Wall, other boxes, and OneWayDoor(wrong direction).
 *           On Ice, box slides until it hits something.
 */
UENUM(BlueprintType)
enum class EPushBoxTileType : uint8
{
	Empty		= 0 UMETA(DisplayName = "Floor"),
	Wall		= 1 UMETA(DisplayName = "Wall"),
	Ice			= 2 UMETA(DisplayName = "Ice"),
	OneWayDoor	= 3 UMETA(DisplayName = "OneWayDoor"),
	PlayerGoal	= 5 UMETA(DisplayName = "PlayerGoal"),
	TargetMarker = 6 UMETA(DisplayName = "TargetMarker"),
};

/** Direction for grid-based movement */
UENUM(BlueprintType)
enum class EPushBoxDirection : uint8
{
	North	UMETA(DisplayName = "North (+Y)"),
	East	UMETA(DisplayName = "East (+X)"),
	South	UMETA(DisplayName = "South (-Y)"),
	West	UMETA(DisplayName = "West (-X)"),
	None	UMETA(DisplayName = "None"),
};

/** The state of a puzzle */
UENUM(BlueprintType)
enum class EPuzzleState : uint8
{
	NotStarted,
	InProgress,
	Completed,
	Failed,
};

/** Editor editing mode (for reverse-design workflow) */
UENUM(BlueprintType)
enum class EPuzzleEditorMode : uint8
{
	/** Place terrain tiles (Wall, Floor, Ice, OneWayDoor) on the grid */
	TilePlacement,
	/** Place targets and boxes on the grid (defines the goal/win state) */
	GoalState,
	/** Rewind mode: move player and reverse-pull boxes to define start positions */
	ReverseEdit,
	/** Play-test the puzzle from start state (forward push) */
	PlayTest,
};

// ============================================================================
// Structs
// ============================================================================

/** Data for a single cell in the puzzle grid */
USTRUCT(BlueprintType)
struct PUSHBOX_API FPushBoxCellData
{
	GENERATED_BODY()

	FPushBoxCellData()
		: TileType(EPushBoxTileType::Empty)
		, bHasBox(false)
		, OneWayDirection(EPushBoxDirection::None)
	{
	}

	/** The base tile type of this cell */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cell")
	EPushBoxTileType TileType;

	/** Whether a box currently sits on this cell */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cell")
	bool bHasBox;

	/** Whether this cell is a target/goal point ¡ª derived from TileType */
	bool IsTarget() const { return TileType == EPushBoxTileType::TargetMarker; }

	/** For OneWayDoor tiles: the allowed passage direction.
	 *  A box can only enter this cell if it is moving in this direction. 
	 *  The player can walk through freely regardless of direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cell")
	EPushBoxDirection OneWayDirection;

	/** GameplayTags for additional properties */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cell")
	FGameplayTagContainer CellTags;
};

/** A single move record for the move history */
USTRUCT(BlueprintType)
struct PUSHBOX_API FPushBoxMoveRecord
{
	GENERATED_BODY()

	FPushBoxMoveRecord()
		: Direction(EPushBoxDirection::None)
		, BoxFrom(FIntPoint::ZeroValue)
		, BoxTo(FIntPoint::ZeroValue)
		, PlayerFrom(FIntPoint::ZeroValue)
		, PlayerTo(FIntPoint::ZeroValue)
		, bIsReverse(false)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	EPushBoxDirection Direction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	FIntPoint BoxFrom;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	FIntPoint BoxTo;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	FIntPoint PlayerFrom;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	FIntPoint PlayerTo;

	/** Was this move recorded during reverse editing? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	bool bIsReverse;
};

/** Snapshot of the entire grid state at a point in time */
USTRUCT(BlueprintType)
struct PUSHBOX_API FPushBoxGridSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snapshot")
	FIntPoint GridSize;

	/** Flat array of cell data, indexed as Y * GridSize.X + X */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snapshot")
	TArray<FPushBoxCellData> Cells;

	/** Current player position on the grid */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snapshot")
	FIntPoint PlayerPosition;

	/** All box positions */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snapshot")
	TArray<FIntPoint> BoxPositions;

	const FPushBoxCellData* GetCell(FIntPoint Coord) const
	{
		if (Coord.X < 0 || Coord.X >= GridSize.X || Coord.Y < 0 || Coord.Y >= GridSize.Y)
			return nullptr;
		int32 Index = Coord.Y * GridSize.X + Coord.X;
		return Cells.IsValidIndex(Index) ? &Cells[Index] : nullptr;
	}

	FPushBoxCellData* GetCellMutable(FIntPoint Coord)
	{
		if (Coord.X < 0 || Coord.X >= GridSize.X || Coord.Y < 0 || Coord.Y >= GridSize.Y)
			return nullptr;
		int32 Index = Coord.Y * GridSize.X + Coord.X;
		return Cells.IsValidIndex(Index) ? &Cells[Index] : nullptr;
	}
};

// ============================================================================
// Gameplay Message Structs (for GameplayMessageRouter)
// ============================================================================

/** Message: A box was moved */
USTRUCT(BlueprintType)
struct PUSHBOX_API FPushBoxMessage_BoxMoved
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Message")
	FIntPoint FromCoord;

	UPROPERTY(BlueprintReadWrite, Category = "Message")
	FIntPoint ToCoord;

	UPROPERTY(BlueprintReadWrite, Category = "Message")
	EPushBoxDirection Direction = EPushBoxDirection::None;
};

/** Message: Puzzle state changed */
USTRUCT(BlueprintType)
struct PUSHBOX_API FPushBoxMessage_PuzzleStateChanged
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Message")
	EPuzzleState OldState = EPuzzleState::NotStarted;

	UPROPERTY(BlueprintReadWrite, Category = "Message")
	EPuzzleState NewState = EPuzzleState::NotStarted;
};

/** Message: Player moved on grid */
USTRUCT(BlueprintType)
struct PUSHBOX_API FPushBoxMessage_PlayerMoved
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Message")
	FIntPoint FromCoord;

	UPROPERTY(BlueprintReadWrite, Category = "Message")
	FIntPoint ToCoord;

	UPROPERTY(BlueprintReadWrite, Category = "Message")
	EPushBoxDirection Direction = EPushBoxDirection::None;
};

// ============================================================================
// Gameplay Tag Constants
// ============================================================================

namespace PushBoxTags
{
	// Message channels
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_BoxMoved);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_PlayerMoved);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_PuzzleStateChanged);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_UndoPerformed);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_ResetPerformed);

	// OwnedTags ¡ª identify what this tile IS
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tile_Floor);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tile_Wall);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tile_Ice);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tile_OneWayDoor);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tile_TargetMarker);

	// UI Widget Stack
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_WidgetStack_Game);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_WidgetStack_Menu);
}
