// Copyright PushBox Game. All Rights Reserved.

#include "ObstacleGenerator.h"
#include "Grid/PushBoxGridManager.h"

// ==================================================================
// Static BFS Utilities
// ==================================================================

TArray<FIntPoint> UObstacleGenerator::BFSFindPath(
	FIntPoint GridSize,
	FIntPoint Start, FIntPoint Goal,
	const TArray<FIntPoint>& BoxPositions,
	const TArray<FPushBoxCellData>* TerrainCells)
{
	if (Start == Goal) return { Start };

	auto IsValid = [&](FIntPoint C) {
		return C.X >= 0 && C.X < GridSize.X && C.Y >= 0 && C.Y < GridSize.Y;
	};
	auto ToIdx = [&](FIntPoint C) { return C.Y * GridSize.X + C.X; };

	if (!IsValid(Start) || !IsValid(Goal)) return {};

	static const FIntPoint Dirs[] = { {0,1}, {0,-1}, {1,0}, {-1,0} };
	const int32 Total = GridSize.X * GridSize.Y;

	TArray<int32> Parent;
	Parent.SetNumUninitialized(Total);
	for (int32 i = 0; i < Total; ++i) Parent[i] = -1;
	Parent[ToIdx(Start)] = ToIdx(Start); // sentinel: self-parent = visited root

	TArray<FIntPoint> Queue;
	Queue.Add(Start);

	while (Queue.Num() > 0)
	{
		FIntPoint Cur = Queue[0];
		Queue.RemoveAt(0, EAllowShrinking::No);

		if (Cur == Goal)
		{
			// Reconstruct path
			TArray<FIntPoint> Path;
			int32 Idx = ToIdx(Goal);
			while (Idx != ToIdx(Start))
			{
				Path.Add(FIntPoint(Idx % GridSize.X, Idx / GridSize.X));
				Idx = Parent[Idx];
			}
			Path.Add(Start);
			Algo::Reverse(Path);
			return Path;
		}

		for (const FIntPoint& D : Dirs)
		{
			FIntPoint Next = Cur + D;
			if (!IsValid(Next)) continue;

			int32 NIdx = ToIdx(Next);
			if (Parent[NIdx] != -1) continue; // already visited

			// Blocked by boxes
			if (BoxPositions.Contains(Next)) continue;

			// Blocked by walls if terrain is provided
			// Player can walk on Ice and OneWayDoor (only boxes are affected)
			if (TerrainCells && TerrainCells->IsValidIndex(NIdx))
			{
				if ((*TerrainCells)[NIdx].TileType == EPushBoxTileType::Wall)
					continue;
			}

			Parent[NIdx] = ToIdx(Cur);
			Queue.Add(Next);
		}
	}

	return {}; // unreachable
}

bool UObstacleGenerator::BFSCanReach(
	FIntPoint GridSize,
	FIntPoint Start, FIntPoint Goal,
	const TArray<FIntPoint>& SimBoxes,
	const TArray<FPushBoxCellData>& Cells)
{
	if (Start == Goal) return true;

	auto IsValid = [&](FIntPoint C) {
		return C.X >= 0 && C.X < GridSize.X && C.Y >= 0 && C.Y < GridSize.Y;
	};
	auto ToIdx = [&](FIntPoint C) { return C.Y * GridSize.X + C.X; };

	if (!IsValid(Start) || !IsValid(Goal)) return false;

	static const FIntPoint Dirs[] = { {0,1}, {0,-1}, {1,0}, {-1,0} };

	TSet<int32> Visited;
	TArray<FIntPoint> Queue;
	Queue.Add(Start);
	Visited.Add(ToIdx(Start));

	while (Queue.Num() > 0)
	{
		FIntPoint Cur = Queue[0];
		Queue.RemoveAt(0, EAllowShrinking::No);

		for (const FIntPoint& D : Dirs)
		{
			FIntPoint Next = Cur + D;
			if (!IsValid(Next)) continue;

			int32 NIdx = ToIdx(Next);
			if (Visited.Contains(NIdx)) continue;

			// Wall blocks
			if (Cells.IsValidIndex(NIdx) && Cells[NIdx].TileType == EPushBoxTileType::Wall)
				continue;

			// Boxes block
			if (SimBoxes.Contains(Next)) continue;

			// Player walks freely on Ice and OneWayDoor
			if (Next == Goal) return true;

			Visited.Add(NIdx);
			Queue.Add(Next);
		}
	}
	return false;
}

FIntPoint UObstacleGenerator::ComputeIceSlide(
	FIntPoint BoxPos, FIntPoint PushDir,
	FIntPoint GridSize,
	const TArray<FIntPoint>& SimBoxes,
	const TArray<FPushBoxCellData>& Cells)
{
	auto IsValid = [&](FIntPoint C) {
		return C.X >= 0 && C.X < GridSize.X && C.Y >= 0 && C.Y < GridSize.Y;
	};
	auto ToIdx = [&](FIntPoint C) { return C.Y * GridSize.X + C.X; };

	FIntPoint SlidePos = BoxPos;
	for (;;)
	{
		FIntPoint Next = SlidePos + PushDir;
		if (!IsValid(Next)) break;

		int32 NIdx = ToIdx(Next);
		if (!Cells.IsValidIndex(NIdx)) break;

		const FPushBoxCellData& NC = Cells[NIdx];
		if (NC.TileType == EPushBoxTileType::Wall) break;
		if (SimBoxes.Contains(Next)) break;

		// OneWayDoor blocks box if direction doesn't match
		// PushDir is the direction the box is moving
		if (NC.TileType == EPushBoxTileType::OneWayDoor)
		{
			// Determine the push direction as EPushBoxDirection
			EPushBoxDirection BoxMoveDir = EPushBoxDirection::None;
			if (PushDir == FIntPoint(0, 1))       BoxMoveDir = EPushBoxDirection::North;
			else if (PushDir == FIntPoint(1, 0))   BoxMoveDir = EPushBoxDirection::East;
			else if (PushDir == FIntPoint(0, -1))  BoxMoveDir = EPushBoxDirection::South;
			else if (PushDir == FIntPoint(-1, 0))  BoxMoveDir = EPushBoxDirection::West;

			if (NC.OneWayDirection != BoxMoveDir) break;
		}

		SlidePos = Next;

		// Stop sliding when we leave ice
		if (Cells[ToIdx(SlidePos)].TileType != EPushBoxTileType::Ice)
			break;
	}
	return SlidePos;
}

// ==================================================================
// Snapshot Recording
// ==================================================================

void UObstacleGenerator::ClearSnapshots()
{
	ReverseSnapshots.Empty();
	SafetyMap.Empty();
	CriticalPathSet.Empty();
	ManeuveringSet.Empty();
	AnalyzedGridSize = FIntPoint::ZeroValue;
}

void UObstacleGenerator::RecordSnapshot(
	const UPushBoxGridManager* GridManager,
	const FPushBoxMoveRecord& Move,
	FIntPoint PrevPlayerPos)
{
	if (!GridManager) return;

	FReverseStepSnapshot Snap;
	Snap.PlayerPosition = GridManager->PlayerGridPosition;
	Snap.BoxPositions = GridManager->BoxPositions;
	Snap.Move = Move;

	// --- Compute the player walk path for the "teleport" ---
	// For a box pull: the player had to walk from PrevPlayerPos to the
	// pull-position (Move.PlayerFrom, which is adjacent to the box).
	// For a player-only move: trivial, no teleport.
	if (Move.BoxFrom != FIntPoint(-1, -1))
	{
		// Box pull: player needed to be at Move.PlayerFrom to execute the pull.
		// If PrevPlayerPos != Move.PlayerFrom, the player walked there first.
		if (PrevPlayerPos != Move.PlayerFrom)
		{
			// BFS from PrevPlayerPos to Move.PlayerFrom
			// At the time of the walk, boxes were in their pre-move positions.
			// The box that's about to be pulled is still at Move.BoxFrom.
			// We reconstruct the box positions before the move:
			TArray<FIntPoint> BoxesBefore = GridManager->BoxPositions;
			// Undo the box move: box is currently at Move.BoxTo, put it back at Move.BoxFrom
			int32 BoxIdx = BoxesBefore.IndexOfByKey(Move.BoxTo);
			if (BoxIdx != INDEX_NONE)
			{
				BoxesBefore[BoxIdx] = Move.BoxFrom;
			}

			Snap.PlayerWalkPath = BFSFindPath(
				GridManager->GridSize,
				PrevPlayerPos, Move.PlayerFrom,
				BoxesBefore,
				nullptr); // no terrain walls yet (early in design)

			if (Snap.PlayerWalkPath.Num() == 0 && PrevPlayerPos != Move.PlayerFrom)
			{
				// Fallback: BFS failed (shouldn't happen on an empty grid)
				// Store direct teleport points so at least they're marked
				Snap.PlayerWalkPath.Add(PrevPlayerPos);
				Snap.PlayerWalkPath.Add(Move.PlayerFrom);
				UE_LOG(LogTemp, Warning,
					TEXT("ObstacleGenerator: BFS walk path failed from (%d,%d) to (%d,%d). "
					     "Storing direct teleport."),
					PrevPlayerPos.X, PrevPlayerPos.Y,
					Move.PlayerFrom.X, Move.PlayerFrom.Y);
			}
		}
	}

	ReverseSnapshots.Add(MoveTemp(Snap));
}

bool UObstacleGenerator::UndoLastSnapshot()
{
	if (ReverseSnapshots.Num() == 0) return false;
	ReverseSnapshots.RemoveAt(ReverseSnapshots.Num() - 1);
	return true;
}

// ==================================================================
// Analysis
// ==================================================================

void UObstacleGenerator::MarkPathAsCritical(const TArray<FIntPoint>& Path)
{
	for (const FIntPoint& P : Path)
	{
		if (IsValidCoord(P))
		{
			CriticalPathSet.Add(CoordToIndex(P));
		}
	}
}

void UObstacleGenerator::AnalyzeSafetyZones(FIntPoint GridSize)
{
	AnalyzedGridSize = GridSize;
	const int32 TotalCells = GridSize.X * GridSize.Y;

	SafetyMap.SetNum(TotalCells);
	CriticalPathSet.Empty();
	ManeuveringSet.Empty();

	for (int32 i = 0; i < TotalCells; ++i)
	{
		SafetyMap[i] = ECellSafetyZone::Unrestricted;
	}

	if (ReverseSnapshots.Num() == 0) return;

	// ---- Pass 1: Mark positions occupied by player or box at any step ----
	for (const FReverseStepSnapshot& Snap : ReverseSnapshots)
	{
		if (IsValidCoord(Snap.PlayerPosition))
			CriticalPathSet.Add(CoordToIndex(Snap.PlayerPosition));

		for (const FIntPoint& BoxPos : Snap.BoxPositions)
		{
			if (IsValidCoord(BoxPos))
				CriticalPathSet.Add(CoordToIndex(BoxPos));
		}

		// Move endpoints
		const FPushBoxMoveRecord& M = Snap.Move;
		if (IsValidCoord(M.PlayerFrom)) CriticalPathSet.Add(CoordToIndex(M.PlayerFrom));
		if (IsValidCoord(M.PlayerTo))   CriticalPathSet.Add(CoordToIndex(M.PlayerTo));
		if (M.BoxFrom != FIntPoint(-1, -1) && IsValidCoord(M.BoxFrom))
			CriticalPathSet.Add(CoordToIndex(M.BoxFrom));
		if (M.BoxTo != FIntPoint(-1, -1) && IsValidCoord(M.BoxTo))
			CriticalPathSet.Add(CoordToIndex(M.BoxTo));

		// ---- KEY FIX: mark the walk path as critical ----
		MarkPathAsCritical(Snap.PlayerWalkPath);
	}

	// Mark critical path cells in the safety map
	for (int32 Idx : CriticalPathSet)
	{
		if (Idx >= 0 && Idx < TotalCells)
		{
			SafetyMap[Idx] = ECellSafetyZone::CriticalPath;
		}
	}

	// ---- Pass 2: Maneuvering zone = neighbors of critical path ----
	static const FIntPoint Dirs[] = { {0,1}, {0,-1}, {1,0}, {-1,0} };

	for (int32 Idx : CriticalPathSet)
	{
		const int32 X = Idx % GridSize.X;
		const int32 Y = Idx / GridSize.X;

		for (const FIntPoint& D : Dirs)
		{
			FIntPoint Neighbor(X + D.X, Y + D.Y);
			if (!IsValidCoord(Neighbor)) continue;

			int32 NIdx = CoordToIndex(Neighbor);
			if (!CriticalPathSet.Contains(NIdx) && !ManeuveringSet.Contains(NIdx))
			{
				ManeuveringSet.Add(NIdx);
				SafetyMap[NIdx] = ECellSafetyZone::Maneuvering;
			}
		}
	}

	// ---- Pass 3: BFS maneuvering between consecutive player positions ----
	// Even with walk paths recorded, the player may need additional space
	// to maneuver around boxes between steps.
	for (int32 i = 1; i < ReverseSnapshots.Num(); ++i)
	{
		const FReverseStepSnapshot& Prev = ReverseSnapshots[i - 1];
		const FReverseStepSnapshot& Curr = ReverseSnapshots[i];

		FPushBoxGridSnapshot TempSnap;
		TempSnap.GridSize = GridSize;
		TempSnap.BoxPositions = Prev.BoxPositions;
		TempSnap.PlayerPosition = Prev.PlayerPosition;

		// BFS from previous player position to current, marking all reachable cells
		TArray<FIntPoint> ReachPath = BFSFindPath(
			GridSize, Prev.PlayerPosition, Curr.Move.PlayerFrom,
			Prev.BoxPositions, nullptr);

		// Mark BFS-reachable area as maneuvering
		for (const FIntPoint& P : ReachPath)
		{
			if (IsValidCoord(P))
			{
				int32 PIdx = CoordToIndex(P);
				if (SafetyMap[PIdx] == ECellSafetyZone::Unrestricted)
				{
					ManeuveringSet.Add(PIdx);
					SafetyMap[PIdx] = ECellSafetyZone::Maneuvering;
				}
			}
		}
	}
}

ECellSafetyZone UObstacleGenerator::GetCellSafetyZone(FIntPoint Coord) const
{
	int32 Idx = Coord.Y * AnalyzedGridSize.X + Coord.X;
	if (Idx >= 0 && Idx < SafetyMap.Num())
		return SafetyMap[Idx];
	return ECellSafetyZone::CriticalPath;
}

TArray<FIntPoint> UObstacleGenerator::GetCriticalPathCells() const
{
	TArray<FIntPoint> Result;
	for (int32 Idx : CriticalPathSet)
		Result.Add(FIntPoint(Idx % AnalyzedGridSize.X, Idx / AnalyzedGridSize.X));
	return Result;
}

TArray<FIntPoint> UObstacleGenerator::GetManeuveringCells() const
{
	TArray<FIntPoint> Result;
	for (int32 Idx : ManeuveringSet)
		Result.Add(FIntPoint(Idx % AnalyzedGridSize.X, Idx / AnalyzedGridSize.X));
	return Result;
}

// ==================================================================
// Generation
// ==================================================================

int32 UObstacleGenerator::GenerateObstacles(
	UPushBoxGridManager* GridManager,
	const FPushBoxGridSnapshot& GoalSnapshot,
	const FObstacleGeneratorConfig& Config)
{
	if (!GridManager || ReverseSnapshots.Num() == 0)
		return 0;

	const FIntPoint GS = GridManager->GridSize;

	// Always recompute safety zones
	AnalyzeSafetyZones(GS);

	FRandomStream Rng(Config.Seed != 0 ? Config.Seed : FMath::Rand());
	int32 PlacedCount = 0;

	// Collect candidate cells (interior only)
	TArray<FIntPoint> Candidates;
	for (int32 Y = 1; Y < GS.Y - 1; ++Y)
	{
		for (int32 X = 1; X < GS.X - 1; ++X)
		{
			FIntPoint Coord(X, Y);
			FPushBoxCellData Cell = GridManager->GetCellData(Coord);
			if (Cell.TileType != EPushBoxTileType::Empty) continue;
			if (Cell.IsTarget() || Cell.bHasBox) continue;
			if (GridManager->PlayerGridPosition == Coord) continue;
			Candidates.Add(Coord);
		}
	}

	// Shuffle candidates for randomness
	for (int32 i = Candidates.Num() - 1; i > 0; --i)
	{
		int32 j = Rng.RandRange(0, i);
		Candidates.Swap(i, j);
	}

	// --- Multi-pass balanced generation ---
	// Instead of greedily filling everything with walls, we use density-based
	// probability rolls per obstacle type. Each cell gets ONE attempt per pass.
	// The designer can call Generate multiple times to add more obstacles.
	//
	// Inspired by AutoGenerateSokobanLevel: use configurable ratios for variety.

	static const EPushBoxDirection AllDirs[] = {
		EPushBoxDirection::North, EPushBoxDirection::East,
		EPushBoxDirection::South, EPushBoxDirection::West
	};

	for (const FIntPoint& Coord : Candidates)
	{
		ECellSafetyZone Zone = GetCellSafetyZone(Coord);
		if (Zone == ECellSafetyZone::CriticalPath)
			continue;

		// Roll to decide WHICH obstacle type to try on this cell
		float Roll = Rng.FRand();

		if (Zone == ECellSafetyZone::Unrestricted)
		{
			// Unrestricted: Wall, Ice, or OneWayDoor
			if (Roll < Config.WallDensity)
			{
				if (TryPlaceObstacle(GridManager, GoalSnapshot, Coord,
					EPushBoxTileType::Wall, EPushBoxDirection::None, true))
				{
					++PlacedCount;
					continue;
				}
			}
			else if (Roll < Config.WallDensity + Config.IceDensity)
			{
				if (TryPlaceObstacle(GridManager, GoalSnapshot, Coord,
					EPushBoxTileType::Ice, EPushBoxDirection::None, true))
				{
					++PlacedCount;
					continue;
				}
			}
			else if (Roll < Config.WallDensity + Config.IceDensity + Config.OneWayDoorDensity)
			{
				EPushBoxDirection Dir = AllDirs[Rng.RandRange(0, 3)];
				if (TryPlaceObstacle(GridManager, GoalSnapshot, Coord,
					EPushBoxTileType::OneWayDoor, Dir, true))
				{
					++PlacedCount;
					continue;
				}
			}
			// else: this cell stays empty this pass (intentional breathing room)
		}
		else if (Zone == ECellSafetyZone::Maneuvering)
		{
			// Maneuvering: NO walls. Ice or OneWayDoor only.
			float ManeuveringRoll = Rng.FRand();
			float IceChance = Config.IceDensity / (Config.IceDensity + Config.OneWayDoorDensity + 0.001f);

			if (ManeuveringRoll < Config.IceDensity)
			{
				if (TryPlaceObstacle(GridManager, GoalSnapshot, Coord,
					EPushBoxTileType::Ice, EPushBoxDirection::None, true))
				{
					++PlacedCount;
					continue;
				}
			}
			else if (ManeuveringRoll < Config.IceDensity + Config.OneWayDoorDensity)
			{
				EPushBoxDirection Dir = AllDirs[Rng.RandRange(0, 3)];
				if (TryPlaceObstacle(GridManager, GoalSnapshot, Coord,
					EPushBoxTileType::OneWayDoor, Dir, true))
				{
					++PlacedCount;
					continue;
				}
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("ObstacleGenerator: Placed %d obstacles on %dx%d grid (%d snapshots, %d candidates)"),
		PlacedCount, GS.X, GS.Y, ReverseSnapshots.Num(), Candidates.Num());

	return PlacedCount;
}

bool UObstacleGenerator::TryPlaceObstacle(
	UPushBoxGridManager* GridManager,
	const FPushBoxGridSnapshot& GoalSnapshot,
	FIntPoint Coord,
	EPushBoxTileType ObstacleType,
	EPushBoxDirection DoorDirection,
	bool bValidate)
{
	if (!GridManager) return false;
	if (WouldBlockSolution(Coord, ObstacleType, DoorDirection))
		return false;

	FPushBoxCellData OldCell = GridManager->GetCellData(Coord);

	FPushBoxCellData NewCell = OldCell;
	NewCell.TileType = ObstacleType;
	if (ObstacleType == EPushBoxTileType::OneWayDoor)
		NewCell.OneWayDirection = DoorDirection;

	GridManager->SetCellData(Coord, NewCell);

	if (bValidate)
	{
		if (!ValidateForwardSolvability(GridManager, GoalSnapshot))
		{
			GridManager->SetCellData(Coord, OldCell);
			return false;
		}
	}

	return true;
}

bool UObstacleGenerator::WouldBlockSolution(
	FIntPoint Coord, EPushBoxTileType ObstacleType, EPushBoxDirection DoorDirection) const
{
	if (!IsValidCoord(Coord)) return true;

	int32 Idx = CoordToIndex(Coord);
	if (CriticalPathSet.Contains(Idx))
		return true;

	if (ObstacleType == EPushBoxTileType::OneWayDoor && ManeuveringSet.Contains(Idx))
	{
		for (const FReverseStepSnapshot& Snap : ReverseSnapshots)
		{
			const FPushBoxMoveRecord& M = Snap.Move;
			if (M.BoxTo == Coord || M.BoxFrom == Coord)
				return true;
		}
	}

	return false;
}

// ==================================================================
// Validation  (BFS-based forward replay)
// ==================================================================

bool UObstacleGenerator::ValidateForwardSolvability(
	const UPushBoxGridManager* GridManager,
	const FPushBoxGridSnapshot& GoalSnapshot) const
{
	if (!GridManager || ReverseSnapshots.Num() == 0)
		return false;

	const FIntPoint GS = GridManager->GridSize;
	FIntPoint SimPlayer = GridManager->PlayerGridPosition;
	TArray<FIntPoint> SimBoxes = GridManager->BoxPositions;

	// Build forward steps by reversing the reverse snapshots.
	// ReverseSnapshots[0] is first reverse move from goal state.
	// ReverseSnapshots[Last] is the last move that created the start state.
	// Forward order: replay from Last to 0.
	for (int32 i = ReverseSnapshots.Num() - 1; i >= 0; --i)
	{
		const FReverseStepSnapshot& Snap = ReverseSnapshots[i];
		const FPushBoxMoveRecord& RevMove = Snap.Move;

		if (RevMove.BoxFrom == FIntPoint(-1, -1))
		{
			// --- Player-only move (forward direction is opposite) ---
			EPushBoxDirection FwdDir = UPushBoxGridManager::GetOppositeDirection(RevMove.Direction);
			FIntPoint FwdOffset = UPushBoxGridManager::GetDirectionOffset(FwdDir);
			FIntPoint NewPos = SimPlayer + FwdOffset;

			if (NewPos.X < 0 || NewPos.X >= GS.X || NewPos.Y < 0 || NewPos.Y >= GS.Y)
				return false;

			int32 Idx = NewPos.Y * GS.X + NewPos.X;
			if (!GridManager->Cells.IsValidIndex(Idx)) return false;
			if (GridManager->Cells[Idx].TileType == EPushBoxTileType::Wall) return false;

			SimPlayer = NewPos;
		}
		else
		{
			// --- Box push ---
			// Reverse pull in direction D => forward push in opposite direction.
			// Forward: player walks to (BoxTo - FwdOffset), then pushes box from BoxTo to BoxFrom.
			// (RevMove.BoxTo is where the box is in the start state / current sim state,
			//  RevMove.BoxFrom is where the box was before the reverse pull = the box's goal.)
			EPushBoxDirection FwdDir = UPushBoxGridManager::GetOppositeDirection(RevMove.Direction);
			FIntPoint FwdOffset = UPushBoxGridManager::GetDirectionOffset(FwdDir);

			FIntPoint BoxCurPos = RevMove.BoxTo;     // box position in sim (current start state)
			FIntPoint BoxDest   = RevMove.BoxFrom;   // where the box needs to go (toward goal)

			// Player needs to be behind the box (opposite of push direction)
			FIntPoint PushPos = BoxCurPos - FwdOffset;

			// 1) BFS: can the player reach PushPos?
			if (!BFSCanReach(GS, SimPlayer, PushPos, SimBoxes, GridManager->Cells))
			{
				// Maybe the player has walk path data to help
				// But even with it, the terrain blocks. Fail.
				return false;
			}

			// 2) Check box destination is legal
			FIntPoint BoxNewPos = BoxCurPos + FwdOffset;
			if (BoxNewPos.X < 0 || BoxNewPos.X >= GS.X ||
				BoxNewPos.Y < 0 || BoxNewPos.Y >= GS.Y)
				return false;

			int32 DestIdx = BoxNewPos.Y * GS.X + BoxNewPos.X;
			if (!GridManager->Cells.IsValidIndex(DestIdx)) return false;

			const FPushBoxCellData& DestCell = GridManager->Cells[DestIdx];
			if (DestCell.TileType == EPushBoxTileType::Wall)
				return false;
			if (DestCell.TileType == EPushBoxTileType::OneWayDoor && DestCell.OneWayDirection != FwdDir)
				return false;
			if (SimBoxes.Contains(BoxNewPos))
				return false;

			// PushPos must also be valid
			if (PushPos.X < 0 || PushPos.X >= GS.X || PushPos.Y < 0 || PushPos.Y >= GS.Y)
				return false;
			int32 PushIdx = PushPos.Y * GS.X + PushPos.X;
			if (GridManager->Cells.IsValidIndex(PushIdx) &&
				GridManager->Cells[PushIdx].TileType == EPushBoxTileType::Wall)
				return false;

			// 3) Execute the push
			int32 BoxArrayIdx = SimBoxes.IndexOfByKey(BoxCurPos);
			if (BoxArrayIdx == INDEX_NONE)
				return false;

			// 4) Handle ice sliding
			FIntPoint FinalBoxPos = BoxNewPos;
			if (DestCell.TileType == EPushBoxTileType::Ice)
			{
				FinalBoxPos = ComputeIceSlide(BoxNewPos, FwdOffset, GS, SimBoxes, GridManager->Cells);
			}

			SimBoxes[BoxArrayIdx] = FinalBoxPos;
			SimPlayer = BoxCurPos; // player steps into box's old position
		}
	}

	// Final check: all boxes on targets
	for (const FIntPoint& BoxPos : SimBoxes)
	{
		if (BoxPos.X < 0 || BoxPos.X >= GS.X || BoxPos.Y < 0 || BoxPos.Y >= GS.Y)
			return false;
		int32 Idx = BoxPos.Y * GS.X + BoxPos.X;
		if (!GridManager->Cells.IsValidIndex(Idx)) return false;
		if (!GridManager->Cells[Idx].IsTarget()) return false;
	}

	return true;
}

// ==================================================================
// Helpers
// ==================================================================

int32 UObstacleGenerator::CoordToIndex(FIntPoint Coord) const
{
	return Coord.Y * AnalyzedGridSize.X + Coord.X;
}

bool UObstacleGenerator::IsValidCoord(FIntPoint Coord) const
{
	return Coord.X >= 0 && Coord.X < AnalyzedGridSize.X
		&& Coord.Y >= 0 && Coord.Y < AnalyzedGridSize.Y;
}
