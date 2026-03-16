// Copyright PushBox Game. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PushBoxTypes.h"
#include "ObstacleGenerator.generated.h"

class UPushBoxGridManager;

/**
 * Lightweight record of the grid state at one step of the reverse-design sequence.
 *
 * IMPORTANT: Each snapshot captures not just the pull/move itself, but also the
 * player's walk path leading up to it. When the designer clicks a distant box,
 * the player "teleports" ¡ª we decompose that into an explicit BFS walk path so
 * that forward replay can verify the player can actually walk there.
 */
USTRUCT(BlueprintType)
struct PUSHBOXEDITOR_API FReverseStepSnapshot
{
	GENERATED_BODY()

	/** Player position AFTER this step */
	UPROPERTY(BlueprintReadOnly, Category = "Snapshot")
	FIntPoint PlayerPosition;

	/** All box positions AFTER this step */
	UPROPERTY(BlueprintReadOnly, Category = "Snapshot")
	TArray<FIntPoint> BoxPositions;

	/** The actual reverse move (pull or player-only move) */
	UPROPERTY(BlueprintReadOnly, Category = "Snapshot")
	FPushBoxMoveRecord Move;

	/**
	 * The player's walk path BEFORE the pull, from the player's previous position
	 * to the required pull-position (adjacent to the box). Empty if the player
	 * was already in position, or for player-only moves.
	 *
	 * Stored as a sequence of grid coordinates the player walks through.
	 * In forward replay this becomes: player walks this path (reversed) to get
	 * into push position.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Snapshot")
	TArray<FIntPoint> PlayerWalkPath;
};


UENUM(BlueprintType)
enum class ECellSafetyZone : uint8
{
	/** Cell is on the critical path ¡ª player or box must pass through it. NO obstacles allowed. */
	CriticalPath,
	/** Cell is adjacent to or reachable from critical path ¡ª player maneuvering space. No walls, but Ice/Door may be ok. */
	Maneuvering,
	/** Cell is not used by the solution at all. Safe for any obstacle. */
	Unrestricted,
};


USTRUCT(BlueprintType)
struct PUSHBOXEDITOR_API FObstacleGeneratorConfig
{
	GENERATED_BODY()

	/** Probability of placing a wall on an unrestricted cell (per pass) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WallDensity = 0.35f;

	/** Probability of placing ice on a valid cell (per pass) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IceDensity = 0.15f;

	/** Probability of placing a one-way door on a valid cell (per pass) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OneWayDoorDensity = 0.10f;

	/** Random seed for reproducible generation (0 = random) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	int32 Seed = 0;

	/** Run forward BFS validation after each obstacle placement */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
	bool bValidatePerStep = true;
};


/**
 * Obstacle generator that analyzes reverse-design snapshots and places
 * walls/ice/one-way doors in positions guaranteed not to break solvability.
 *
 * ## The Teleport Problem
 *
 * In multi-box puzzles, after finishing one box the designer clicks a different
 * box far away. The player "teleports" to the new box's pull-position. In the
 * forward solution this translates to: "player walks from current position to
 * the push-position for the next box". If we place a wall on that walk path,
 * the puzzle becomes unsolvable.
 *
 * Solution: When recording a snapshot, we run BFS from the player's old position
 * to the required pull-position (avoiding boxes) and store the full walk path.
 * All cells on this walk path become CriticalPath cells.
 *
 * ## Validation Strategy (inspired by YASS solver / Sokoban literature)
 *
 * Forward replay decomposes each solution step into:
 *   1. BFS: can the player reach the push-position through current terrain?
 *   2. Push: is the box's destination cell legal (wall/door/ice checks)?
 *   3. Ice slide: compute final box resting position.
 *
 * This catches obstacles that the simple "replay recorded moves" approach misses.
 *
 * ## Player vs Ice/OneWayDoor Rules (game-specific)
 *
 * - Player can walk on Ice and OneWayDoor cells freely (no sliding, no direction check).
 * - Only boxes are affected by Ice (slide) and OneWayDoor (direction gate).
 */
UCLASS()
class PUSHBOXEDITOR_API UObstacleGenerator : public UObject
{
	GENERATED_BODY()

public:
	// ------------------------------------------------------------------
	// Snapshot Recording
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "ObstacleGenerator")
	void ClearSnapshots();

	/**
	 * Record a reverse step. Automatically computes the player's walk path
	 * from the previous snapshot's player position to the required pull-position.
	 *
	 * @param GridManager  Current grid state (AFTER the move was applied)
	 * @param Move         The move record from GridManager->MoveHistory
	 * @param PrevPlayerPos  Player position BEFORE the move (before any teleport)
	 */
	UFUNCTION(BlueprintCallable, Category = "ObstacleGenerator")
	void RecordSnapshot(const UPushBoxGridManager* GridManager,
	                    const FPushBoxMoveRecord& Move,
	                    FIntPoint PrevPlayerPos);

	UFUNCTION(BlueprintPure, Category = "ObstacleGenerator")
	int32 GetSnapshotCount() const { return ReverseSnapshots.Num(); }

	const TArray<FReverseStepSnapshot>& GetSnapshots() const { return ReverseSnapshots; }

	UFUNCTION(BlueprintCallable, Category = "ObstacleGenerator")
	bool UndoLastSnapshot();

	// ------------------------------------------------------------------
	// Analysis
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "ObstacleGenerator")
	void AnalyzeSafetyZones(FIntPoint GridSize);

	UFUNCTION(BlueprintPure, Category = "ObstacleGenerator")
	ECellSafetyZone GetCellSafetyZone(FIntPoint Coord) const;

	UFUNCTION(BlueprintCallable, Category = "ObstacleGenerator")
	TArray<FIntPoint> GetCriticalPathCells() const;

	UFUNCTION(BlueprintCallable, Category = "ObstacleGenerator")
	TArray<FIntPoint> GetManeuveringCells() const;

	// ------------------------------------------------------------------
	// Generation
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "ObstacleGenerator")
	int32 GenerateObstacles(UPushBoxGridManager* GridManager,
	                        const FPushBoxGridSnapshot& GoalSnapshot,
	                        const FObstacleGeneratorConfig& Config);

	// ------------------------------------------------------------------
	// Validation
	// ------------------------------------------------------------------

	/**
	 * Full forward BFS validation. For each solution step:
	 * 1. BFS: verify player can reach the push-position through current terrain
	 * 2. Verify the push itself is legal (wall, door direction, ice slide)
	 * 3. Verify all boxes end up on targets at the end
	 *
	 * This is the definitive solvability check.
	 */
	UFUNCTION(BlueprintCallable, Category = "ObstacleGenerator")
	bool ValidateForwardSolvability(const UPushBoxGridManager* GridManager,
	                                const FPushBoxGridSnapshot& GoalSnapshot) const;

private:
	UPROPERTY()
	TArray<FReverseStepSnapshot> ReverseSnapshots;

	TArray<ECellSafetyZone> SafetyMap;
	FIntPoint AnalyzedGridSize;

	TSet<int32> CriticalPathSet;
	TSet<int32> ManeuveringSet;

	// Helpers
	int32 CoordToIndex(FIntPoint Coord) const;
	bool IsValidCoord(FIntPoint Coord) const;

	/**
	 * BFS from Start to Goal, avoiding boxes. Returns the path as a sequence
	 * of coordinates (including Start and Goal), or empty if unreachable.
	 *
	 * @param GridSize    Grid dimensions
	 * @param BoxPositions  Current box positions (block the player)
	 * @param WallCells   Additional cells to treat as walls (for terrain-aware BFS)
	 */
	static TArray<FIntPoint> BFSFindPath(FIntPoint GridSize,
	                                     FIntPoint Start, FIntPoint Goal,
	                                     const TArray<FIntPoint>& BoxPositions,
	                                     const TArray<FPushBoxCellData>* TerrainCells = nullptr);

	/**
	 * BFS reachability check (no path needed, just yes/no).
	 * Player is blocked by walls and boxes. Ice and OneWayDoor are walkable.
	 */
	static bool BFSCanReach(FIntPoint GridSize,
	                        FIntPoint Start, FIntPoint Goal,
	                        const TArray<FIntPoint>& SimBoxes,
	                        const TArray<FPushBoxCellData>& Cells);

	/** Compute final box position after ice sliding. */
	static FIntPoint ComputeIceSlide(FIntPoint BoxPos, FIntPoint PushDir,
	                                 FIntPoint GridSize,
	                                 const TArray<FIntPoint>& SimBoxes,
	                                 const TArray<FPushBoxCellData>& Cells);

	/** Mark cells on a walk path as critical. */
	void MarkPathAsCritical(const TArray<FIntPoint>& Path);

	/** Try to place a single obstacle. Returns true if placed AND validated. */
	bool TryPlaceObstacle(UPushBoxGridManager* GridManager,
	                      const FPushBoxGridSnapshot& GoalSnapshot,
	                      FIntPoint Coord,
	                      EPushBoxTileType ObstacleType,
	                      EPushBoxDirection DoorDirection,
	                      bool bValidate);

	bool WouldBlockSolution(FIntPoint Coord, EPushBoxTileType ObstacleType,
	                        EPushBoxDirection DoorDirection) const;
};
