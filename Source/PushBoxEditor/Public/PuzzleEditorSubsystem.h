// Copyright PushBox Game. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "PushBoxTypes.h"
#include "ObstacleGenerator.h"
#include "MCTSPuzzleGenerator.h"
#include "PuzzleEditorSubsystem.generated.h"

class UPuzzleLevelDataAsset;
class UPushBoxGridManager;
class APushBoxLevelActor;
class APlayerStart;
class UWFCAsset;

/**
 * Editor subsystem that manages the reverse-design puzzle editing workflow.
 *
 * Workflow for the designer:
 *
 * 1. CreateNewPuzzle() ！ creates a PuzzleLevelDataAsset + blank grid
 * 2. SetMode(GoalState) ！ designer places box targets (goal points) on the empty grid
 * 3. SetMode(ReverseEdit) ！ designer reverse-pulls boxes to define start positions
 *    Each pull is recorded as a snapshot for solvability analysis.
 * 4. GenerateObstacles() ！ system analyzes snapshots, places walls/ice/doors on safe cells
 * 5. ValidateAndSave() ！ validates the puzzle, computes forward solution, saves DataAsset
 * 6. At runtime, the game loads StartState from the DataAsset for the player.
 */
UCLASS()
class PUSHBOXEDITOR_API UPuzzleEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ------------------------------------------------------------------
	// Workflow
	// ------------------------------------------------------------------

	/** Create a new puzzle data asset and spawn a PushBoxLevelActor in the current level */
	UFUNCTION(BlueprintCallable, Category = "PuzzleEditor")
	UPuzzleLevelDataAsset* CreateNewPuzzle(FString PuzzleName, FIntPoint GridSize);

	/** Load an existing puzzle data asset into the editor for editing or preview */
	UFUNCTION(BlueprintCallable, Category = "PuzzleEditor")
	void LoadPuzzleData(UPuzzleLevelDataAsset* InPuzzleData);

	/** Run terrain generation on the active level actor */
	UFUNCTION(BlueprintCallable, Category = "PuzzleEditor")
	bool GenerateTerrain(int32 Seed = 0);

	/** Set the current editing mode */
	UFUNCTION(BlueprintCallable, Category = "PuzzleEditor")
	void SetEditMode(EPuzzleEditorMode NewMode);

	/** Get current editing mode */
	UFUNCTION(BlueprintPure, Category = "PuzzleEditor")
	EPuzzleEditorMode GetEditMode() const { return CurrentMode; }

	/** Remove a box from a cell */
	UFUNCTION(BlueprintCallable, Category = "PuzzleEditor")
	bool RemoveBox(FIntPoint Coord);

	/** Set player start position */
	UFUNCTION(BlueprintCallable, Category = "PuzzleEditor")
	void SetPlayerStart(FIntPoint Coord);

	/** Set cell tile type */
	UFUNCTION(BlueprintCallable, Category = "PuzzleEditor")
	void SetCellType(FIntPoint Coord, EPushBoxTileType Type);

	/** Set OneWayDoor allowed direction on a cell */
	UFUNCTION(BlueprintCallable, Category = "PuzzleEditor")
	void SetCellOneWayDirection(FIntPoint Coord, EPushBoxDirection Direction);

	/** Reverse-pull a box (ReverseEdit mode) */
	UFUNCTION(BlueprintCallable, Category = "PuzzleEditor")
	bool ReversePullBox(FIntPoint BoxCoord, EPushBoxDirection Direction);

	/** Move the player in reverse-edit mode (without pushing boxes) */
	UFUNCTION(BlueprintCallable, Category = "PuzzleEditor")
	bool ReversePlayerMove(EPushBoxDirection Direction);

	/** Undo last reverse pull */
	UFUNCTION(BlueprintCallable, Category = "PuzzleEditor")
	bool UndoReversePull();

	/** Validate the puzzle and save to ActivePuzzleData.
	 *  ActivePuzzleData must already be set (via LoadPuzzleData or CreateNewPuzzle).
	 *  If validation fails, shows a force-save dialog. */
	UFUNCTION(BlueprintCallable, Category = "PuzzleEditor")
	bool ValidateAndSave(FText& OutError);

	// ------------------------------------------------------------------
	// Obstacle Generation (solvability-guaranteed)
	// ------------------------------------------------------------------

	/** Generate obstacles (walls/ice/doors) that are guaranteed not to break solvability.
	 *  Must be called AFTER the designer finishes reverse-pulling boxes.
	 *  Analyzes the recorded reverse snapshots to determine safe placement zones.
	 *
	 *  @param Config  Generation parameters (densities, seed, validation mode)
	 *  @return Number of obstacles placed
	 */
	UFUNCTION(BlueprintCallable, Category = "PuzzleEditor|Obstacles")
	int32 GenerateObstacles(const FObstacleGeneratorConfig& Config);

	/** Get the obstacle generator instance (for visualization/debug). */
	UFUNCTION(BlueprintPure, Category = "PuzzleEditor|Obstacles")
	UObstacleGenerator* GetObstacleGenerator() const { return ObstacleGen; }

	/** Check if the current puzzle state is solvable via forward replay.
	 *  Returns true if the recorded reverse sequence can be replayed forward. */
	UFUNCTION(BlueprintCallable, Category = "PuzzleEditor|Obstacles")
	bool CheckSolvability() const;

	// ------------------------------------------------------------------
	// MCTS Auto-Generation
	// ------------------------------------------------------------------

	/** Fully automatic puzzle generation using Monte Carlo Tree Search.
	 *  Generates room layout, box targets, start positions, player position,
	 *  and obstacles ！ all guaranteed to be solvable.
	 *
	 *  @param Config  MCTS generation parameters
	 *  @return Result containing goal state, start state, and solution
	 */
	UFUNCTION(BlueprintCallable, Category = "PuzzleEditor|MCTS")
	FMCTSResult MCTSAutoGenerate(const FMCTSConfig& Config);

	// ------------------------------------------------------------------
	// Grid Operations
	// ------------------------------------------------------------------

	/** Clear the grid: revert all interior cells to Empty floor, keep outer wall border.
	 *  Also clears boxes, targets, move history, and goal snapshot. */
	UFUNCTION(BlueprintCallable, Category = "PuzzleEditor")
	void ClearGrid();

	/** Resize the grid to a new dimension and clear all content.
	 *  Outer border becomes Wall, interior becomes Empty. */
	UFUNCTION(BlueprintCallable, Category = "PuzzleEditor")
	void ResizeGrid(FIntPoint NewSize);

	// ------------------------------------------------------------------
	// Accessors
	// ------------------------------------------------------------------

	/** Get the active level actor */
	UFUNCTION(BlueprintPure, Category = "PuzzleEditor")
	APushBoxLevelActor* GetActiveLevelActor() const { return ActiveLevelActor; }

	/** Get the active grid manager */
	UFUNCTION(BlueprintPure, Category = "PuzzleEditor")
	UPushBoxGridManager* GetActiveGridManager() const;

	/** Get the active puzzle data asset */
	UFUNCTION(BlueprintPure, Category = "PuzzleEditor")
	UPuzzleLevelDataAsset* GetActivePuzzleData() const { return ActivePuzzleData; }

	/** Get the goal state snapshot (captured when switching Goal★Reverse) */
	const FPushBoxGridSnapshot& GetGoalSnapshot() const { return GoalSnapshot; }

	/** Set the goal state snapshot (used when restoring state for "Save as new") */
	void SetGoalSnapshot(const FPushBoxGridSnapshot& InSnapshot) { GoalSnapshot = InSnapshot; }

	/** Set the tile asset reference on the active puzzle and sync grid dimensions */
	UFUNCTION(BlueprintCallable, Category = "PuzzleEditor")
	void SetWFCAsset(UWFCAsset* InWFCAsset);

	/** Sync the tile asset's grid dimensions to match the puzzle grid size, and save it */
	UFUNCTION(BlueprintCallable, Category = "PuzzleEditor")
	void SyncWFCGridSize(FIntPoint NewGridSize);

	// ------------------------------------------------------------------
	// State
	// ------------------------------------------------------------------

	/** Set the active level actor (e.g., when user selects one in outliner) */
	UFUNCTION(BlueprintCallable, Category = "PuzzleEditor")
	void SetActiveLevelActor(APushBoxLevelActor* InActor);

	/** Try to restore the last editing session (loads saved DataAsset path from config) */
	void TryRestoreLastSession();

	/** Save the current session path to config so it can be restored later */
	void SaveSessionToConfig() const;

	/** Move the level's PlayerStart actor to match the grid player position */
	void SyncPlayerStartToGrid();

private:
	UPROPERTY()
	TObjectPtr<APushBoxLevelActor> ActiveLevelActor;

	UPROPERTY()
	TObjectPtr<UPuzzleLevelDataAsset> ActivePuzzleData;

	/** Standalone grid manager for editor-only editing (no level actor needed) */
	UPROPERTY()
	TObjectPtr<UPushBoxGridManager> EditorGridManager;

	UPROPERTY()
	TObjectPtr<UObstacleGenerator> ObstacleGen;

	EPuzzleEditorMode CurrentMode = EPuzzleEditorMode::TilePlacement;

	/** Snapshot of goal state taken when switching from GoalState to ReverseEdit */
	FPushBoxGridSnapshot GoalSnapshot;

	/** Find or spawn the PlayerStart actor in the current editor world */
	APlayerStart* FindOrSpawnPlayerStart() const;

	/** BFS check: can the player walk from Start to Goal? */
	static bool CanPlayerWalkTo(const UPushBoxGridManager* GM, FIntPoint Start, FIntPoint Goal);

	/** Create or find a DataAsset with the given name under /Game/PushBox/Puzzles/ */
	UPuzzleLevelDataAsset* CreateOrFindDataAsset(const FString& AssetName);

	/** Save a DataAsset to disk, ensuring directory exists. Returns false on failure. */
	bool SaveDataAssetToDisk(UPuzzleLevelDataAsset* Asset, FText& OutError);

	/** Find the APushBoxLevelActor in the current editor world (first one found).
	 *  Caches it in ActiveLevelActor if found. */
	APushBoxLevelActor* FindAndBindLevelActor();

	/** Sync ActivePuzzleData to the level actor's PuzzleLevelData reference. */
	void SyncLevelActorDataAsset();
};
