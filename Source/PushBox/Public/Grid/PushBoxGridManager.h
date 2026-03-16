// Copyright PushBox Game. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PushBoxTypes.h"
#include "PushBoxGridManager.generated.h"

class UPuzzleLevelDataAsset;
class UWFCGeneratorComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPuzzleStateChanged, EPuzzleState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBoxMoved, FIntPoint, From, FIntPoint, To);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerMoved, FIntPoint, From, FIntPoint, To);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPuzzleCompleted);

/**
 * Core puzzle logic component. Attach to the level actor to manage
 * push-box gameplay logic on top of the tile-based terrain grid.
 *
 * This component reads the 2D grid (cell size, dimensions) from the
 * sibling UWFCGeneratorComponent, and manages the logical puzzle state
 * (box positions, player position, move history, win detection).
 *
 * At runtime it loads from a UPuzzleLevelDataAsset.
 * In the editor it drives the reverse-design workflow.
 */
UCLASS(ClassGroup = (PushBox), meta = (BlueprintSpawnableComponent))
class PUSHBOX_API UPushBoxGridManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UPushBoxGridManager();

	// ------------------------------------------------------------------
	// Configuration
	// ------------------------------------------------------------------

	/** Internal reference to the puzzle data asset. Set programmatically.
	 *  Use APushBoxLevelActor::PuzzleLevelData to assign the puzzle asset in the editor. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Puzzle")
	TObjectPtr<UPuzzleLevelDataAsset> PuzzleData;

	/** Grid dimensions (read from PuzzleData or tile grid) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid")
	FIntPoint GridSize;

	/** World-space size of one cell in cm (matched to grid CellSize) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	FVector2D CellSize = FVector2D(200.0, 200.0);

	// ------------------------------------------------------------------
	// Runtime State
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "State")
	EPuzzleState CurrentPuzzleState;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	FIntPoint PlayerGridPosition;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	TArray<FIntPoint> BoxPositions;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	TArray<FPushBoxCellData> Cells;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	TArray<FPushBoxMoveRecord> MoveHistory;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	int32 MoveCount;

	// ------------------------------------------------------------------
	// Events
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnPuzzleStateChanged OnPuzzleStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnBoxMoved OnBoxMoved;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnPlayerMoved OnPlayerMoved;

	/** Fired when all boxes are on their target cells (puzzle completed).
	 *  Bind to this in Blueprint to trigger win screen / results UI. */
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnPuzzleCompleted OnPuzzleCompleted;

	// ------------------------------------------------------------------
	// Initialization
	// ------------------------------------------------------------------

	/** Initialize grid from puzzle data asset (runtime) */
	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void InitializeFromDataAsset(UPuzzleLevelDataAsset* InPuzzleData);

	/** Initialize a blank grid of given size (editor tool usage) */
	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void InitializeBlankGrid(FIntPoint InGridSize);

	/** Sync grid dimensions and cell size from sibling WFCGeneratorComponent */
	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void SyncWithWFCGenerator();

	// ------------------------------------------------------------------
	// Gameplay - Forward (Player pushes boxes)
	// ------------------------------------------------------------------

	/** Attempt to move the player in a direction. Returns true if move was valid. */
	UFUNCTION(BlueprintCallable, Category = "Gameplay")
	bool TryMovePlayer(EPushBoxDirection Direction);

	/** Undo the last move */
	UFUNCTION(BlueprintCallable, Category = "Gameplay")
	bool UndoLastMove();

	/** Reset puzzle to start state */
	UFUNCTION(BlueprintCallable, Category = "Gameplay")
	void ResetToStart();

	// ------------------------------------------------------------------
	// Gameplay - Reverse (Designer pulls boxes in editor)
	// ------------------------------------------------------------------

	/** Attempt to reverse-pull a box. The player moves to the opposite side of the box
	 *  and pulls it one cell in the given direction. Returns true if valid. */
	UFUNCTION(BlueprintCallable, Category = "Editor")
	bool TryReversePullBox(FIntPoint BoxCoord, EPushBoxDirection PullDirection);

	/** Move the player one cell in a direction (reverse edit - no box pushing).
	 *  Used to position the player separately from boxes in reverse design. */
	UFUNCTION(BlueprintCallable, Category = "Editor")
	bool TryReversePlayerMove(EPushBoxDirection Direction);

	// ------------------------------------------------------------------
	// Queries
	// ------------------------------------------------------------------

	/** Get cell data at a grid coordinate */
	UFUNCTION(BlueprintPure, Category = "Grid")
	FPushBoxCellData GetCellData(FIntPoint Coord) const;

	/** Set cell data at a grid coordinate */
	UFUNCTION(BlueprintCallable, Category = "Grid")
	void SetCellData(FIntPoint Coord, const FPushBoxCellData& NewData);

	/** Check if a coordinate is within grid bounds */
	UFUNCTION(BlueprintPure, Category = "Grid")
	bool IsValidCoord(FIntPoint Coord) const;

	/** Check if a cell is walkable by the PLAYER.
	 *  Player is blocked only by Wall and by boxes. Everything else is passable. */
	UFUNCTION(BlueprintPure, Category = "Grid")
	bool IsCellWalkable(FIntPoint Coord) const;

	/** Check if a box can be pushed INTO this cell from the given direction.
	 *  Box is blocked by Wall, other boxes, and OneWayDoor (wrong direction).
	 *  Special behavior on Ice (slide) and Portal (teleport) handled separately. */
	UFUNCTION(BlueprintPure, Category = "Grid")
	bool CanBoxEnterCell(FIntPoint Coord, EPushBoxDirection PushDirection) const;

	/** Check if a box exists at a coordinate */
	UFUNCTION(BlueprintPure, Category = "Grid")
	bool HasBoxAt(FIntPoint Coord) const;

	/** Convert grid coordinate to world position */
	UFUNCTION(BlueprintPure, Category = "Grid")
	FVector GridToWorld(FIntPoint Coord) const;

	/** Convert world position to grid coordinate */
	UFUNCTION(BlueprintPure, Category = "Grid")
	FIntPoint WorldToGrid(FVector WorldPos) const;

	/** Take a snapshot of the current state */
	UFUNCTION(BlueprintCallable, Category = "Grid")
	FPushBoxGridSnapshot TakeSnapshot() const;

	/** Check if all boxes are on target cells (win condition) */
	UFUNCTION(BlueprintPure, Category = "Gameplay")
	bool CheckWinCondition() const;

	/** Get the direction vector as grid offset */
	UFUNCTION(BlueprintPure, Category = "Grid")
	static FIntPoint GetDirectionOffset(EPushBoxDirection Direction);

	/** Get the opposite direction */
	UFUNCTION(BlueprintPure, Category = "Grid")
	static EPushBoxDirection GetOppositeDirection(EPushBoxDirection Direction);

	/** Change puzzle state and broadcast */
	UFUNCTION(BlueprintCallable, Category = "Gameplay")
	void SetPuzzleState(EPuzzleState NewState);

protected:
	virtual void BeginPlay() override;

private:
	int32 CoordToIndex(FIntPoint Coord) const;
	void BroadcastBoxMoved(FIntPoint From, FIntPoint To, EPushBoxDirection Dir);
	void BroadcastPlayerMoved(FIntPoint From, FIntPoint To, EPushBoxDirection Dir);

	/** Cached reference to sibling tile generator component */
	UPROPERTY()
	TObjectPtr<UWFCGeneratorComponent> CachedWFCGenerator;
};
