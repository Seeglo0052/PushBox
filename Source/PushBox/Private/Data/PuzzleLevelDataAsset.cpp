// Copyright PushBox Game. All Rights Reserved.

#include "Data/PuzzleLevelDataAsset.h"
#include "Grid/PushBoxGridManager.h"
#include "Utilities/PushBoxFunctionLibrary.h"

// ============================================================================
// Helper: simulate one forward move on a snapshot (returns false if illegal)
// ============================================================================
namespace PuzzleValidation
{
	// Check coordinate validity
	static bool IsValid(FIntPoint Coord, FIntPoint GridSize)
	{
		return Coord.X >= 0 && Coord.X < GridSize.X && Coord.Y >= 0 && Coord.Y < GridSize.Y;
	}

	static int32 ToIndex(FIntPoint Coord, int32 SizeX)
	{
		return Coord.Y * SizeX + Coord.X;
	}

	static FIntPoint DirOffset(EPushBoxDirection Dir)
	{
		switch (Dir)
		{
		case EPushBoxDirection::North: return FIntPoint( 0,  1);
		case EPushBoxDirection::East:  return FIntPoint( 1,  0);
		case EPushBoxDirection::South: return FIntPoint( 0, -1);
		case EPushBoxDirection::West:  return FIntPoint(-1,  0);
		default:                       return FIntPoint( 0,  0);
		}
	}

	static bool HasBox(const TArray<FIntPoint>& Boxes, FIntPoint Coord)
	{
		return Boxes.Contains(Coord);
	}

	static bool IsCellWalkable(const FPushBoxGridSnapshot& Snap, FIntPoint Coord, const TArray<FIntPoint>& Boxes)
	{
		if (!IsValid(Coord, Snap.GridSize)) return false;
		const FPushBoxCellData& Cell = Snap.Cells[ToIndex(Coord, Snap.GridSize.X)];
		if (Cell.TileType == EPushBoxTileType::Wall) return false;
		if (HasBox(Boxes, Coord)) return false;
		return true;
	}

	static bool CanBoxEnter(const FPushBoxGridSnapshot& Snap, FIntPoint Coord,
		EPushBoxDirection PushDir, const TArray<FIntPoint>& Boxes)
	{
		if (!IsValid(Coord, Snap.GridSize)) return false;
		const FPushBoxCellData& Cell = Snap.Cells[ToIndex(Coord, Snap.GridSize.X)];
		if (Cell.TileType == EPushBoxTileType::Wall) return false;
		if (HasBox(Boxes, Coord)) return false;
		if (Cell.TileType == EPushBoxTileType::OneWayDoor && Cell.OneWayDirection != PushDir)
			return false;
		return true;
	}

	// Simulate a single forward move. Returns false if move is illegal.
	// A move can be either:
	//   1. Player walks into an empty cell (player blocked only by walls)
	//   2. Player pushes a box (box blocked by walls, other boxes, wrong-direction OneWayDoor)
	static bool SimulateMove(const FPushBoxGridSnapshot& Snap,
		FIntPoint& PlayerPos, TArray<FIntPoint>& Boxes,
		EPushBoxDirection Dir)
	{
		const FIntPoint Off = DirOffset(Dir);
		const FIntPoint NewPlayerPos = PlayerPos + Off;

		if (!IsValid(NewPlayerPos, Snap.GridSize)) return false;
		const FPushBoxCellData& TargetCell = Snap.Cells[ToIndex(NewPlayerPos, Snap.GridSize.X)];

		// Player is ONLY blocked by walls (not by Ice, OneWayDoor, etc.)
		if (TargetCell.TileType == EPushBoxTileType::Wall) return false;

		if (HasBox(Boxes, NewPlayerPos))
		{
			// Push the box
			const FIntPoint NewBoxPos = NewPlayerPos + Off;
			if (!CanBoxEnter(Snap, NewBoxPos, Dir, Boxes)) return false;

			int32 BoxIdx = Boxes.IndexOfByKey(NewPlayerPos);
			if (BoxIdx == INDEX_NONE) return false;

			// Handle ice sliding
			FIntPoint FinalBoxPos = NewBoxPos;
			const FPushBoxCellData& BoxDest = Snap.Cells[ToIndex(NewBoxPos, Snap.GridSize.X)];
			if (BoxDest.TileType == EPushBoxTileType::Ice)
			{
				FIntPoint SlidePos = NewBoxPos;
				for (;;)
				{
					FIntPoint Next = SlidePos + Off;
					if (!IsValid(Next, Snap.GridSize)) break;
					const FPushBoxCellData& NC = Snap.Cells[ToIndex(Next, Snap.GridSize.X)];
					if (NC.TileType == EPushBoxTileType::Wall || HasBox(Boxes, Next)) break;
					if (NC.TileType == EPushBoxTileType::OneWayDoor && NC.OneWayDirection != Dir) break;
					SlidePos = Next;
					if (Snap.Cells[ToIndex(SlidePos, Snap.GridSize.X)].TileType != EPushBoxTileType::Ice)
						break;
				}
				FinalBoxPos = SlidePos;
			}

			Boxes[BoxIdx] = FinalBoxPos;
		}
		// else: simple walk ！ no box interaction, always allowed if not a wall

		PlayerPos = NewPlayerPos;
		return true;
	}

	// BFS reachability check for the player (ignoring boxes, just walls).
	// Returns the number of non-wall cells reachable from Start.
	static int32 FloodFillReachable(const FPushBoxGridSnapshot& Snap, FIntPoint Start)
	{
		if (!IsValid(Start, Snap.GridSize)) return 0;
		const int32 SX = Snap.GridSize.X;

		TSet<int32> Visited;
		TArray<FIntPoint> Queue;
		Queue.Add(Start);
		Visited.Add(ToIndex(Start, SX));

		static const FIntPoint Dirs[] = { {0,1}, {0,-1}, {1,0}, {-1,0} };

		while (Queue.Num() > 0)
		{
			FIntPoint Cur = Queue[0];
			Queue.RemoveAt(0, EAllowShrinking::No);

			for (const FIntPoint& D : Dirs)
			{
				FIntPoint Next = Cur + D;
				if (!IsValid(Next, Snap.GridSize)) continue;
				int32 NIdx = ToIndex(Next, SX);
				if (Visited.Contains(NIdx)) continue;
				const FPushBoxCellData& Cell = Snap.Cells[NIdx];
				if (Cell.TileType == EPushBoxTileType::Wall) continue;
				Visited.Add(NIdx);
				Queue.Add(Next);
			}
		}

		return Visited.Num();
	}

	// Check if player can reach ANY of the given coordinates (BFS, ignoring boxes)
	static bool CanPlayerReachAny(const FPushBoxGridSnapshot& Snap, FIntPoint Start, const TArray<FIntPoint>& Targets)
	{
		if (!IsValid(Start, Snap.GridSize)) return false;
		const int32 SX = Snap.GridSize.X;

		TSet<int32> Visited;
		TArray<FIntPoint> Queue;
		Queue.Add(Start);
		Visited.Add(ToIndex(Start, SX));

		// Check start itself
		if (Targets.Contains(Start)) return true;

		static const FIntPoint Dirs[] = { {0,1}, {0,-1}, {1,0}, {-1,0} };

		while (Queue.Num() > 0)
		{
			FIntPoint Cur = Queue[0];
			Queue.RemoveAt(0, EAllowShrinking::No);

			for (const FIntPoint& D : Dirs)
			{
				FIntPoint Next = Cur + D;
				if (!IsValid(Next, Snap.GridSize)) continue;
				int32 NIdx = ToIndex(Next, SX);
				if (Visited.Contains(NIdx)) continue;
				const FPushBoxCellData& Cell = Snap.Cells[NIdx];
				if (Cell.TileType == EPushBoxTileType::Wall) continue;
				Visited.Add(NIdx);
				Queue.Add(Next);
				if (Targets.Contains(Next)) return true;
			}
		}
		return false;
	}
}

// ============================================================================
// ValidatePuzzle
// ============================================================================

bool UPuzzleLevelDataAsset::ValidatePuzzle(FText& OutError) const
{
	using namespace PuzzleValidation;

	// ---- Structural checks ----

	if (GridSize.X < 3 || GridSize.Y < 3)
	{
		OutError = FText::FromString(TEXT("Grid size must be at least 3x3."));
		return false;
	}

	if (StartState.Cells.Num() != GridSize.X * GridSize.Y)
	{
		OutError = FText::FromString(FString::Printf(TEXT("StartState cell count (%d) does not match grid size (%dx%d = %d)."),
			StartState.Cells.Num(), GridSize.X, GridSize.Y, GridSize.X * GridSize.Y));
		return false;
	}

	if (GoalState.Cells.Num() != GridSize.X * GridSize.Y)
	{
		OutError = FText::FromString(FString::Printf(TEXT("GoalState cell count (%d) does not match grid size (%dx%d = %d)."),
			GoalState.Cells.Num(), GridSize.X, GridSize.Y, GridSize.X * GridSize.Y));
		return false;
	}

	if (StartState.BoxPositions.Num() == 0)
	{
		OutError = FText::FromString(TEXT("Puzzle must have at least one box."));
		return false;
	}

	if (StartState.BoxPositions.Num() != GoalState.BoxPositions.Num())
	{
		OutError = FText::FromString(TEXT("Start and goal states must have the same number of boxes."));
		return false;
	}

	// ---- Player start position check ----

	const FPushBoxCellData* PlayerCell = StartState.GetCell(StartState.PlayerPosition);
	if (!PlayerCell || PlayerCell->TileType == EPushBoxTileType::Wall)
	{
		OutError = FText::FromString(TEXT("Player start position is invalid (on a wall or out of bounds)."));
		return false;
	}

	// ---- Goal state: all boxes must be on target cells ----

	for (const FIntPoint& BoxPos : GoalState.BoxPositions)
	{
		const FPushBoxCellData* Cell = GoalState.GetCell(BoxPos);
		if (!Cell || !Cell->IsTarget())
		{
			OutError = FText::Format(
				FText::FromString(TEXT("Goal state box at ({0},{1}) is not on a target cell.")),
				FText::AsNumber(BoxPos.X), FText::AsNumber(BoxPos.Y));
			return false;
		}
	}

	// ---- Boxes must NOT all be on targets in StartState (puzzle trivially solved) ----

	int32 BoxesAlreadyOnTarget = 0;
	for (const FIntPoint& BoxPos : StartState.BoxPositions)
	{
		const FPushBoxCellData* Cell = StartState.GetCell(BoxPos);
		if (Cell && Cell->IsTarget())
		{
			++BoxesAlreadyOnTarget;
		}
	}

	if (BoxesAlreadyOnTarget == StartState.BoxPositions.Num())
	{
		OutError = FText::FromString(
			TEXT("All boxes are already on their targets! The puzzle is solved from the start.\n"
				 "Switch to 'Reverse Edit' mode and pull at least one box off its target."));
		return false;
	}

	// ---- Start and goal box positions must differ ----

	{
		bool bAnyDifference = false;
		for (int32 i = 0; i < StartState.BoxPositions.Num(); ++i)
		{
			if (!GoalState.BoxPositions.Contains(StartState.BoxPositions[i]))
			{
				bAnyDifference = true;
				break;
			}
		}
		if (!bAnyDifference)
		{
			for (int32 i = 0; i < GoalState.BoxPositions.Num(); ++i)
			{
				if (!StartState.BoxPositions.Contains(GoalState.BoxPositions[i]))
				{
					bAnyDifference = true;
					break;
				}
			}
		}
		if (!bAnyDifference)
		{
			OutError = FText::FromString(
				TEXT("Start and goal box positions are identical ！ the puzzle has no moves!\n"
					 "Use 'Reverse Edit' mode to pull boxes away from their targets."));
			return false;
		}
	}

	// ================================================================
	// REACHABILITY: Player must not be completely trapped
	// ================================================================

	{
		int32 ReachableCount = FloodFillReachable(StartState, StartState.PlayerPosition);
		if (ReachableCount <= 1)
		{
			OutError = FText::FromString(
				TEXT("Player is completely trapped! The player start position is surrounded by walls\n"
					 "and cannot move at all. Please open a path."));
			return false;
		}
	}

	// ================================================================
	// REACHABILITY: Player must be able to reach at least one box
	// (ignoring box collisions ！ just checking static walls/terrain)
	// ================================================================

	{
		// Gather all cells adjacent to boxes (push positions)
		TArray<FIntPoint> BoxAdjacentCells;
		static const FIntPoint Dirs[] = { {0,1}, {0,-1}, {1,0}, {-1,0} };
		for (const FIntPoint& BoxPos : StartState.BoxPositions)
		{
			for (const FIntPoint& D : Dirs)
			{
				FIntPoint Adj = BoxPos + D;
				if (IsValid(Adj, GridSize))
				{
					const FPushBoxCellData* AdjCell = StartState.GetCell(Adj);
					if (AdjCell && AdjCell->TileType != EPushBoxTileType::Wall)
					{
						BoxAdjacentCells.AddUnique(Adj);
					}
				}
			}
		}

		if (!CanPlayerReachAny(StartState, StartState.PlayerPosition, BoxAdjacentCells))
		{
			OutError = FText::FromString(
				TEXT("Player cannot reach any box! The player start position is separated from\n"
					 "all boxes by walls. Please ensure there is a path from the player to at least one box."));
			return false;
		}
	}

	// ================================================================
	// REACHABILITY: All target cells must be in the same connected
	// region as the player (ignoring boxes)
	// ================================================================

	{
		TArray<FIntPoint> TargetCells;
		for (int32 i = 0; i < GoalState.Cells.Num(); ++i)
		{
			if (GoalState.Cells[i].IsTarget())
			{
				int32 TX = i % GridSize.X;
				int32 TY = i / GridSize.X;
				TargetCells.Add(FIntPoint(TX, TY));
			}
		}

		if (TargetCells.Num() > 0 && !CanPlayerReachAny(StartState, StartState.PlayerPosition, TargetCells))
		{
			OutError = FText::FromString(
				TEXT("Target cells are unreachable from the player start position!\n"
					 "Ensure there is a path (through non-wall terrain) from the player to the target area."));
			return false;
		}
	}

	// ================================================================
	// SOLUTION REPLAY: Verify the recorded solution actually works
	// ================================================================
	//
	// The solution moves are high-level push operations derived from
	// reverse-editing. Each move is: "player pushes box at BoxFrom in
	// Direction, box ends at BoxTo". Between pushes the player must
	// be able to WALK (via BFS) to the push position. We validate:
	//   1. Player can BFS-reach the push position
	//   2. The push itself is legal (box destination not blocked)
	//   3. Ice sliding is handled
	//   4. After all moves, all boxes are on targets

	if (SolutionMoves.Num() > 0)
	{
		FIntPoint SimPlayer = StartState.PlayerPosition;
		TArray<FIntPoint> SimBoxes = StartState.BoxPositions;

		for (int32 i = 0; i < SolutionMoves.Num(); ++i)
		{
			const FPushBoxMoveRecord& Move = SolutionMoves[i];
			const FIntPoint Off = DirOffset(Move.Direction);

			if (Move.BoxFrom == FIntPoint(-1, -1))
			{
				// Player-only move (no box push) ！ just verify destination is walkable
				const FIntPoint NewPos = SimPlayer + Off;
				if (!IsCellWalkable(StartState, NewPos, SimBoxes))
				{
					OutError = FText::FromString(FString::Printf(
						TEXT("Solution replay failed at move %d/%d: player walk blocked."),
						i + 1, SolutionMoves.Num()));
					return false;
				}
				SimPlayer = NewPos;
			}
			else
			{
				// Box push: player must reach the cell behind the box (push position)
				const FIntPoint PushPos = Move.BoxFrom - Off;

				// BFS: can the player walk from SimPlayer to PushPos?
				if (SimPlayer != PushPos)
				{
					bool bCanReach = false;
					{
						const int32 SX = StartState.GridSize.X;
						TSet<int32> Visited;
						TArray<FIntPoint> Queue;
						Queue.Add(SimPlayer);
						Visited.Add(ToIndex(SimPlayer, SX));

						static const FIntPoint BFSDirs[] = { {0,1}, {0,-1}, {1,0}, {-1,0} };
						while (Queue.Num() > 0 && !bCanReach)
						{
							FIntPoint Cur = Queue[0];
							Queue.RemoveAt(0, EAllowShrinking::No);

							for (const FIntPoint& D : BFSDirs)
							{
								FIntPoint Next = Cur + D;
								if (!IsValid(Next, StartState.GridSize)) continue;
								int32 NIdx = ToIndex(Next, SX);
								if (Visited.Contains(NIdx)) continue;
								const FPushBoxCellData& Cell = StartState.Cells[NIdx];
								if (Cell.TileType == EPushBoxTileType::Wall) continue;
								if (HasBox(SimBoxes, Next)) continue;
								Visited.Add(NIdx);
								if (Next == PushPos) { bCanReach = true; break; }
								Queue.Add(Next);
							}
						}
					}

					if (!bCanReach)
					{
						OutError = FText::FromString(FString::Printf(
							TEXT("Solution replay failed at move %d/%d: player cannot reach push position (%d,%d) -> (%d,%d)."),
							i + 1, SolutionMoves.Num(), SimPlayer.X, SimPlayer.Y, PushPos.X, PushPos.Y));
						return false;
					}
				}

				// Verify push position is valid terrain
				if (!IsValid(PushPos, StartState.GridSize))
				{
					OutError = FText::FromString(FString::Printf(
						TEXT("Solution replay failed at move %d/%d: push position out of bounds."),
						i + 1, SolutionMoves.Num()));
					return false;
				}
				const FPushBoxCellData& PushCell = StartState.Cells[ToIndex(PushPos, StartState.GridSize.X)];
				if (PushCell.TileType == EPushBoxTileType::Wall)
				{
					OutError = FText::FromString(FString::Printf(
						TEXT("Solution replay failed at move %d/%d: push position is a wall."),
						i + 1, SolutionMoves.Num()));
					return false;
				}

				// Verify box is at BoxFrom
				int32 BoxIdx = SimBoxes.IndexOfByKey(Move.BoxFrom);
				if (BoxIdx == INDEX_NONE)
				{
					OutError = FText::FromString(FString::Printf(
						TEXT("Solution replay failed at move %d/%d: no box at (%d,%d)."),
						i + 1, SolutionMoves.Num(), Move.BoxFrom.X, Move.BoxFrom.Y));
					return false;
				}

				// Verify box destination is legal
				const FIntPoint BoxDest = Move.BoxFrom + Off;
				if (!CanBoxEnter(StartState, BoxDest, Move.Direction, SimBoxes))
				{
					OutError = FText::FromString(FString::Printf(
						TEXT("Solution replay failed at move %d/%d: box cannot enter (%d,%d)."),
						i + 1, SolutionMoves.Num(), BoxDest.X, BoxDest.Y));
					return false;
				}

				// Handle ice sliding
				FIntPoint FinalBoxPos = BoxDest;
				if (IsValid(BoxDest, StartState.GridSize))
				{
					const FPushBoxCellData& DestCell = StartState.Cells[ToIndex(BoxDest, StartState.GridSize.X)];
					if (DestCell.TileType == EPushBoxTileType::Ice)
					{
						FIntPoint SlidePos = BoxDest;
						for (;;)
						{
							FIntPoint Next = SlidePos + Off;
							if (!IsValid(Next, StartState.GridSize)) break;
							const FPushBoxCellData& NC = StartState.Cells[ToIndex(Next, StartState.GridSize.X)];
							if (NC.TileType == EPushBoxTileType::Wall || HasBox(SimBoxes, Next)) break;
							if (NC.TileType == EPushBoxTileType::OneWayDoor && NC.OneWayDirection != Move.Direction) break;
							SlidePos = Next;
							if (StartState.Cells[ToIndex(SlidePos, StartState.GridSize.X)].TileType != EPushBoxTileType::Ice)
								break;
						}
						FinalBoxPos = SlidePos;
					}
				}

				// Execute the push
				SimBoxes[BoxIdx] = FinalBoxPos;
				SimPlayer = Move.BoxFrom; // player steps into where box was
			}
		}

		// After replaying all moves, check if all boxes are on targets
		bool bAllOnTargets = true;
		for (const FIntPoint& BoxPos : SimBoxes)
		{
			const FPushBoxCellData* Cell = GoalState.GetCell(BoxPos);
			if (!Cell || !Cell->IsTarget())
			{
				bAllOnTargets = false;
				break;
			}
		}

		if (!bAllOnTargets)
		{
			OutError = FText::FromString(
				TEXT("Solution replay completed but not all boxes ended up on targets!\n"
					 "The solution does not achieve the goal state. Please redo reverse-editing."));
			return false;
		}
	}
	else
	{
		// No solution moves recorded
		OutError = FText::FromString(
			TEXT("No solution moves recorded. The puzzle has not been reverse-edited.\n"
				 "Please use Reverse Edit mode to pull boxes and generate a solution."));
		return false;
	}

	OutError = FText::GetEmpty();
	return true;
}
