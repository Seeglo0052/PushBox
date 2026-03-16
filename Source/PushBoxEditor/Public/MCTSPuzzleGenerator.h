// Copyright PushBox Game. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PushBoxTypes.h"
#include "MCTSPuzzleGenerator.generated.h"

class UPushBoxGridManager;

// ============================================================================
// MCTS Configuration
// ============================================================================

USTRUCT(BlueprintType)
struct PUSHBOXEDITOR_API FMCTSConfig
{
	GENERATED_BODY()

	/** Grid dimensions for the generated puzzle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCTS", meta = (ClampMin = "4", ClampMax = "20"))
	FIntPoint GridSize = FIntPoint(7, 7);

	/** Number of boxes to place */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCTS", meta = (ClampMin = "1", ClampMax = "8"))
	int32 NumBoxes = 3;

	/** Time budget in seconds for the MCTS search */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCTS", meta = (ClampMin = "1.0", ClampMax = "120.0"))
	float TimeBudgetSeconds = 10.0f;

	/** Maximum number of MCTS simulations (0 = unlimited, time-based only) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCTS", meta = (ClampMin = "0"))
	int32 MaxSimulations = 0;

	/** Exploration constant C in UCB formula. sqrt(2) is standard. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCTS", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float ExplorationConstant = 1.414f;

	/** Minimum reverse pulls required (ensures non-trivial puzzles) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCTS", meta = (ClampMin = "2", ClampMax = "50"))
	int32 MinReversePulls = 4;

	/** Maximum reverse pulls during shuffling phase */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCTS", meta = (ClampMin = "5", ClampMax = "100"))
	int32 MaxReversePulls = 30;

	/** Minimum Manhattan distance each box must be from its target at start.
	 *  Prevents boxes from spawning on or right next to their targets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCTS", meta = (ClampMin = "2", ClampMax = "10"))
	int32 MinBoxDisplacement = 3;

	/** Weight for boxes in congestion rectangles (alpha in paper) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCTS|Evaluation")
	float CongestionBoxWeight = 4.0f;

	/** Weight for goals in congestion rectangles (beta in paper) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCTS|Evaluation")
	float CongestionGoalWeight = 4.0f;

	/** Weight for obstacles in congestion rectangles (gamma in paper) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCTS|Evaluation")
	float CongestionObstacleWeight = 1.0f;

	/** Normalization constant k for evaluation function */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCTS|Evaluation")
	float NormalizationK = 100.0f;

	/** Random seed (0 = random) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCTS")
	int32 Seed = 0;

	/** Probability of placing walls on unrestricted non-maneuvering cells.
	 *  Keep low ¡ª the room carver already provides the basic layout. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCTS|Obstacles", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WallDensity = 0.10f;

	/** Probability of placing ice during obstacle generation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCTS|Obstacles", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IceDensity = 0.15f;

	/** Probability of placing one-way doors during obstacle generation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCTS|Obstacles", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OneWayDoorDensity = 0.10f;

	/** Ratio of interior cells to carve as walkable floor (0.0¨C1.0).
	 *  Higher values ¡ú more open space. For medium maps (13¡Á13), 0.85 is good. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCTS|Room", meta = (ClampMin = "0.5", ClampMax = "1.0"))
	float FloorRatio = 0.85f;
};

// ============================================================================
// MCTS Result
// ============================================================================

USTRUCT(BlueprintType)
struct PUSHBOXEDITOR_API FMCTSResult
{
	GENERATED_BODY()

	/** Whether the generation succeeded */
	UPROPERTY(BlueprintReadOnly, Category = "MCTS")
	bool bSuccess = false;

	/** The evaluation score of the best puzzle found */
	UPROPERTY(BlueprintReadOnly, Category = "MCTS")
	float BestScore = 0.f;

	/** Number of MCTS simulations performed */
	UPROPERTY(BlueprintReadOnly, Category = "MCTS")
	int32 SimulationCount = 0;

	/** Time spent searching (seconds) */
	UPROPERTY(BlueprintReadOnly, Category = "MCTS")
	float ElapsedSeconds = 0.f;

	/** The goal state snapshot (boxes on targets) */
	UPROPERTY(BlueprintReadOnly, Category = "MCTS")
	FPushBoxGridSnapshot GoalState;

	/** The start state snapshot (boxes at starting positions, player placed) */
	UPROPERTY(BlueprintReadOnly, Category = "MCTS")
	FPushBoxGridSnapshot StartState;

	/** The reverse move sequence that was found (reverse pulls from goal to start) */
	UPROPERTY(BlueprintReadOnly, Category = "MCTS")
	TArray<FPushBoxMoveRecord> ReverseMoves;

	/** The forward solution (reversed from ReverseMoves) */
	UPROPERTY(BlueprintReadOnly, Category = "MCTS")
	TArray<FPushBoxMoveRecord> ForwardSolution;
};

// ============================================================================
// Internal MCTS Node
// ============================================================================

/**
 * Represents a state in the MCTS search tree for puzzle generation.
 * Not a UObject ¡ª lightweight for fast allocation during search.
 */
struct FMCTSNode
{
	/** Grid cells state */
	TArray<FPushBoxCellData> Cells;
	FIntPoint GridSize;

	/** Box positions (goal positions initially, then shuffled to start positions) */
	TArray<FIntPoint> BoxPositions;

	/** Target positions (where boxes must end up ¡ª the goal) */
	TArray<FIntPoint> TargetPositions;

	/** Player position */
	FIntPoint PlayerPosition;

	/** Reverse moves accumulated so far (shuffling phase) */
	TArray<FPushBoxMoveRecord> ReverseMoves;

	/** Player walk paths for each reverse move (player must walk to pull position).
	 *  PlayerWalkPaths[i] = BFS path the player walks before ReverseMoves[i]. */
	TArray<TArray<FIntPoint>> PlayerWalkPaths;

	/** Phase: true = still in initialization, false = shuffling */
	bool bInitPhase = true;

	/** The action index that led to this node from parent */
	int32 ActionIndex = -1;

	// --- MCTS bookkeeping ---
	FMCTSNode* Parent = nullptr;
	TArray<TUniquePtr<FMCTSNode>> Children;

	/** Total accumulated score from rollouts through this node */
	double TotalScore = 0.0;

	/** Number of times this node has been visited */
	int32 VisitCount = 0;

	/** Whether all children have been expanded */
	bool bFullyExpanded = false;

	/** Cache of untried action indices */
	TArray<int32> UntriedActions;

	/** UCB1 value for selection */
	double GetUCBValue(float C) const;

	/** Deep copy the state (not tree structure) */
	void CopyStateFrom(const FMCTSNode& Other);
};

// ============================================================================
// MCTS Puzzle Generator
// ============================================================================

/**
 * Monte Carlo Tree Search based puzzle generator.
 *
 * Implements the approach from:
 *   "Generating Sokoban Puzzle Game Levels with Monte Carlo Tree Search"
 *   (Kartal, Sohre, Guy ¡ª University of Minnesota)
 *
 * The key insight: by generating puzzles through simulated reverse gameplay,
 * every generated puzzle is inherently solvable. MCTS optimizes the evaluation
 * function to find interesting, non-trivial puzzles.
 *
 * Two-phase generation:
 * 1. Initialization: carve room layout, place boxes on targets
 * 2. Shuffling: reverse-pull boxes to create start positions
 *
 * After MCTS finishes, obstacles (walls/ice/doors) are placed on unrestricted
 * cells to enhance the terrain, validated to not break solvability.
 */
UCLASS()
class PUSHBOXEDITOR_API UMCTSPuzzleGenerator : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Run the MCTS search and return the best puzzle found.
	 * This is a synchronous (blocking) call.
	 */
	UFUNCTION(BlueprintCallable, Category = "MCTS")
	FMCTSResult Generate(const FMCTSConfig& Config);

private:
	// --- MCTS Phases ---

	/** Build the root node: carve a room layout and place boxes on targets */
	TUniquePtr<FMCTSNode> BuildRootState(const FMCTSConfig& Config, FRandomStream& Rng);

	/** Carve a connected room shape from the fully-walled grid */
	void CarveRoom(FMCTSNode& Node, const FMCTSConfig& Config, FRandomStream& Rng);

	/** Place boxes on random floor tiles and mark them as targets */
	void PlaceBoxesOnTargets(FMCTSNode& Node, int32 NumBoxes, FRandomStream& Rng);

	/** Place the player on a random reachable floor tile */
	void PlacePlayer(FMCTSNode& Node, FRandomStream& Rng);

	// --- MCTS Core ---

	/** One MCTS iteration: select ¡ú expand ¡ú rollout ¡ú backpropagate */
	void MCTSIteration(FMCTSNode* Root, const FMCTSConfig& Config, FRandomStream& Rng);

	/** Selection phase: walk down tree using UCB1 */
	FMCTSNode* Select(FMCTSNode* Node, float C);

	/** Expansion phase: add a new child node */
	FMCTSNode* Expand(FMCTSNode* Node, FRandomStream& Rng, const FMCTSConfig& Config);

	/** Rollout phase: random simulation to terminal state, return score */
	double Rollout(const FMCTSNode* Node, const FMCTSConfig& Config, FRandomStream& Rng);

	/** Backpropagation: update scores up the tree */
	void Backpropagate(FMCTSNode* Node, double Score);

	// --- Action Enumeration ---

	/** Get all valid reverse-pull actions from a given state */
	struct FPullAction
	{
		FIntPoint BoxCoord;
		EPushBoxDirection PullDirection;
	};
	TArray<FPullAction> GetValidPullActions(const FMCTSNode& Node) const;

	// --- Simulation Helpers ---

	/** Apply a reverse pull to a node state (in-place), returns false if invalid */
	bool ApplyReversePull(FMCTSNode& Node, const FPullAction& Action) const;

	/** Check if a coordinate is valid and not a wall */
	bool IsWalkable(const FMCTSNode& Node, FIntPoint Coord) const;

	/** Check if a box exists at coord */
	bool HasBoxAt(const FMCTSNode& Node, FIntPoint Coord) const;

	/** BFS: can player walk from A to B, avoiding boxes and walls? */
	bool CanPlayerReach(const FMCTSNode& Node, FIntPoint From, FIntPoint To) const;

	/** BFS: find path from A to B, avoiding boxes and walls. Returns empty if unreachable. */
	TArray<FIntPoint> BFSFindPath(const FMCTSNode& Node, FIntPoint From, FIntPoint To) const;

	/** Get all floor cells reachable by player */
	TArray<FIntPoint> GetReachableFloorCells(const FMCTSNode& Node, FIntPoint From) const;

	// --- Evaluation (from paper Equation 2) ---

	/** Evaluate a completed puzzle state. Returns score in [0, 1]. */
	double Evaluate(const FMCTSNode& Node, const FMCTSConfig& Config) const;

	/** Terrain metric: sum of wall neighbors for all floor cells */
	double ComputeTerrain(const FMCTSNode& Node) const;

	/** Congestion metric: box-path interaction density */
	double ComputeCongestion(const FMCTSNode& Node, const FMCTSConfig& Config) const;

	// --- Post-Processing ---

	/** Place additional obstacles on unrestricted cells, validating solvability */
	void PostProcessObstacles(FMCTSNode& Node, const FMCTSConfig& Config, FRandomStream& Rng);

	/** Try placing ice/doors on critical path cells to make special tiles part of the solution */
	void PostProcessCriticalPathSpecials(FMCTSNode& Node, const FMCTSConfig& Config, FRandomStream& Rng);

	/** Collect all critical cells (solution path + walk paths + ice slides + doors) */
	void CollectCriticalCells(const FMCTSNode& Node, TSet<int32>& OutCritical) const;

	/** Validate that the reverse move sequence can be replayed forward */
	bool ValidateForward(const FMCTSNode& Node) const;

	// --- Utility ---
	int32 CoordToIndex(FIntPoint GridSize, FIntPoint Coord) const;
	bool IsValidCoord(FIntPoint GridSize, FIntPoint Coord) const;
};
