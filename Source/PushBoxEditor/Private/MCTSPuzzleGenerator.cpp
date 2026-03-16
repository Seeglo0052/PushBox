// Copyright PushBox Game. All Rights Reserved.

#include "MCTSPuzzleGenerator.h"
#include "Grid/PushBoxGridManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogMCTS, Log, All);

// ============================================================================
// FMCTSNode
// ============================================================================

double FMCTSNode::GetUCBValue(float C) const
{
	if (VisitCount == 0) return TNumericLimits<double>::Max();
	if (!Parent) return 0.0;

	const double Exploitation = TotalScore / static_cast<double>(VisitCount);
	const double Exploration = C * FMath::Sqrt(FMath::Loge(static_cast<double>(Parent->VisitCount))
		/ static_cast<double>(VisitCount));
	return Exploitation + Exploration;
}

void FMCTSNode::CopyStateFrom(const FMCTSNode& Other)
{
	Cells = Other.Cells;
	GridSize = Other.GridSize;
	BoxPositions = Other.BoxPositions;
	TargetPositions = Other.TargetPositions;
	PlayerPosition = Other.PlayerPosition;
	ReverseMoves = Other.ReverseMoves;
	PlayerWalkPaths = Other.PlayerWalkPaths;
	bInitPhase = Other.bInitPhase;
}

// ============================================================================
// Utility
// ============================================================================

int32 UMCTSPuzzleGenerator::CoordToIndex(FIntPoint GridSize, FIntPoint Coord) const
{
	return Coord.Y * GridSize.X + Coord.X;
}

bool UMCTSPuzzleGenerator::IsValidCoord(FIntPoint GridSize, FIntPoint Coord) const
{
	return Coord.X >= 0 && Coord.X < GridSize.X && Coord.Y >= 0 && Coord.Y < GridSize.Y;
}

bool UMCTSPuzzleGenerator::IsWalkable(const FMCTSNode& Node, FIntPoint Coord) const
{
	if (!IsValidCoord(Node.GridSize, Coord)) return false;
	return Node.Cells[CoordToIndex(Node.GridSize, Coord)].TileType != EPushBoxTileType::Wall;
}

bool UMCTSPuzzleGenerator::HasBoxAt(const FMCTSNode& Node, FIntPoint Coord) const
{
	return Node.BoxPositions.Contains(Coord);
}

// ============================================================================
// BFS Helpers
// ============================================================================

TArray<FIntPoint> UMCTSPuzzleGenerator::BFSFindPath(const FMCTSNode& Node, FIntPoint From, FIntPoint To) const
{
	if (From == To) return { From };
	if (!IsValidCoord(Node.GridSize, From) || !IsValidCoord(Node.GridSize, To)) return {};

	static const FIntPoint Dirs[] = { {0,1}, {0,-1}, {1,0}, {-1,0} };
	const int32 Total = Node.GridSize.X * Node.GridSize.Y;

	TArray<int32> Parent;
	Parent.SetNumUninitialized(Total);
	for (int32 i = 0; i < Total; ++i) Parent[i] = -1;
	int32 StartIdx = CoordToIndex(Node.GridSize, From);
	Parent[StartIdx] = StartIdx;

	TArray<FIntPoint> Queue;
	Queue.Add(From);

	while (Queue.Num() > 0)
	{
		FIntPoint Cur = Queue[0];
		Queue.RemoveAt(0, EAllowShrinking::No);

		if (Cur == To)
		{
			// Reconstruct path
			TArray<FIntPoint> Path;
			int32 Idx = CoordToIndex(Node.GridSize, To);
			while (Idx != StartIdx)
			{
				Path.Add(FIntPoint(Idx % Node.GridSize.X, Idx / Node.GridSize.X));
				Idx = Parent[Idx];
			}
			Path.Add(From);
			Algo::Reverse(Path);
			return Path;
		}

		for (const FIntPoint& D : Dirs)
		{
			FIntPoint Next = Cur + D;
			if (!IsValidCoord(Node.GridSize, Next)) continue;
			int32 NIdx = CoordToIndex(Node.GridSize, Next);
			if (Parent[NIdx] != -1) continue;
			if (Node.Cells[NIdx].TileType == EPushBoxTileType::Wall) continue;
			if (HasBoxAt(Node, Next)) continue;

			Parent[NIdx] = CoordToIndex(Node.GridSize, Cur);
			Queue.Add(Next);
		}
	}
	return {}; // unreachable
}

bool UMCTSPuzzleGenerator::CanPlayerReach(const FMCTSNode& Node, FIntPoint From, FIntPoint To) const
{
	if (From == To) return true;
	return BFSFindPath(Node, From, To).Num() > 0;
}

TArray<FIntPoint> UMCTSPuzzleGenerator::GetReachableFloorCells(const FMCTSNode& Node, FIntPoint From) const
{
	TArray<FIntPoint> Result;
	if (!IsValidCoord(Node.GridSize, From)) return Result;

	static const FIntPoint Dirs[] = { {0,1}, {0,-1}, {1,0}, {-1,0} };
	const int32 Total = Node.GridSize.X * Node.GridSize.Y;

	TArray<bool> Visited;
	Visited.SetNumZeroed(Total);
	Visited[CoordToIndex(Node.GridSize, From)] = true;
	Result.Add(From);

	TArray<FIntPoint> Queue;
	Queue.Add(From);

	while (Queue.Num() > 0)
	{
		FIntPoint Cur = Queue[0];
		Queue.RemoveAt(0, EAllowShrinking::No);

		for (const FIntPoint& D : Dirs)
		{
			FIntPoint Next = Cur + D;
			if (!IsValidCoord(Node.GridSize, Next)) continue;
			int32 NIdx = CoordToIndex(Node.GridSize, Next);
			if (Visited[NIdx]) continue;
			if (Node.Cells[NIdx].TileType == EPushBoxTileType::Wall) continue;
			if (HasBoxAt(Node, Next)) continue;

			Visited[NIdx] = true;
			Result.Add(Next);
			Queue.Add(Next);
		}
	}
	return Result;
}

// ============================================================================
// Action Enumeration
// ============================================================================

TArray<UMCTSPuzzleGenerator::FPullAction> UMCTSPuzzleGenerator::GetValidPullActions(const FMCTSNode& Node) const
{
	TArray<FPullAction> Actions;

	static const EPushBoxDirection AllDirs[] = {
		EPushBoxDirection::North,
		EPushBoxDirection::East,
		EPushBoxDirection::South,
		EPushBoxDirection::West
	};

	for (const FIntPoint& BoxPos : Node.BoxPositions)
	{
		for (EPushBoxDirection Dir : AllDirs)
		{
			FIntPoint Off = UPushBoxGridManager::GetDirectionOffset(Dir);
			FIntPoint PlayerPos = BoxPos + Off;      // Where player must stand
			FIntPoint PlayerNewPos = PlayerPos + Off; // Where player moves to
			FIntPoint BoxNewPos = BoxPos + Off;       // = PlayerPos, where box moves to

			// Check all required cells are valid and walkable
			if (!IsValidCoord(Node.GridSize, PlayerPos)) continue;
			if (!IsValidCoord(Node.GridSize, PlayerNewPos)) continue;

			if (Node.Cells[CoordToIndex(Node.GridSize, PlayerPos)].TileType == EPushBoxTileType::Wall) continue;
			if (Node.Cells[CoordToIndex(Node.GridSize, PlayerNewPos)].TileType == EPushBoxTileType::Wall) continue;

			// Box destination must not be a wall
			if (Node.Cells[CoordToIndex(Node.GridSize, BoxNewPos)].TileType == EPushBoxTileType::Wall) continue;

			// NOTE: In reverse mode, OneWayDoor does NOT block box entry ！ any direction is valid.
			// So we intentionally skip the OneWayDoor direction check here.

			// Cannot move box onto another box
			if (HasBoxAt(Node, BoxNewPos) && BoxNewPos != BoxPos) continue;

			// Player must be able to reach the pull position
			if (!CanPlayerReach(Node, Node.PlayerPosition, PlayerPos)) continue;

			Actions.Add({ BoxPos, Dir });
		}
	}

	return Actions;
}

// ============================================================================
// Simulation: Apply Reverse Pull
// ============================================================================

bool UMCTSPuzzleGenerator::ApplyReversePull(FMCTSNode& Node, const FPullAction& Action) const
{
	FIntPoint Off = UPushBoxGridManager::GetDirectionOffset(Action.PullDirection);
	FIntPoint PlayerPos = Action.BoxCoord + Off;
	FIntPoint PlayerNewPos = PlayerPos + Off;
	FIntPoint BoxNewPos = Action.BoxCoord + Off; // = PlayerPos

	// Find the box
	int32 BoxIdx = Node.BoxPositions.IndexOfByKey(Action.BoxCoord);
	if (BoxIdx == INDEX_NONE) return false;

	// Record player walk path BEFORE the pull (from current position to required pull position)
	TArray<FIntPoint> WalkPath;
	if (Node.PlayerPosition != PlayerPos)
	{
		WalkPath = BFSFindPath(Node, Node.PlayerPosition, PlayerPos);
	}
	Node.PlayerWalkPaths.Add(WalkPath);

	// Ice sliding: if the box lands on ice, it slides in the pull direction
	FIntPoint FinalBoxPos = BoxNewPos;
	int32 DestIdx = CoordToIndex(Node.GridSize, BoxNewPos);
	if (IsValidCoord(Node.GridSize, BoxNewPos) && Node.Cells[DestIdx].TileType == EPushBoxTileType::Ice)
	{
		FIntPoint SlidePos = BoxNewPos;
		for (;;)
		{
			FIntPoint Next = SlidePos + Off;
			if (!IsValidCoord(Node.GridSize, Next)) break;
			int32 NIdx = CoordToIndex(Node.GridSize, Next);
			if (Node.Cells[NIdx].TileType == EPushBoxTileType::Wall) break;
			if (HasBoxAt(Node, Next)) break;
			// In reverse, OneWayDoor does not block sliding
			SlidePos = Next;
			if (Node.Cells[CoordToIndex(Node.GridSize, SlidePos)].TileType != EPushBoxTileType::Ice) break;
		}
		FinalBoxPos = SlidePos;
	}

	// Move box
	Node.BoxPositions[BoxIdx] = FinalBoxPos;

	// Record the move
	FPushBoxMoveRecord Rec;
	Rec.Direction = Action.PullDirection;
	Rec.PlayerFrom = PlayerPos;
	Rec.PlayerTo = PlayerNewPos;
	Rec.BoxFrom = Action.BoxCoord;
	Rec.BoxTo = FinalBoxPos;
	Rec.bIsReverse = true;
	Node.ReverseMoves.Add(Rec);

	// Update player position
	Node.PlayerPosition = PlayerNewPos;

	return true;
}

// ============================================================================
// Evaluation Function (Paper Equation 2)
// ============================================================================

double UMCTSPuzzleGenerator::ComputeTerrain(const FMCTSNode& Node) const
{
	static const FIntPoint Dirs[] = { {0,1}, {0,-1}, {1,0}, {-1,0} };
	double Terrain = 0.0;

	for (int32 Y = 0; Y < Node.GridSize.Y; ++Y)
	{
		for (int32 X = 0; X < Node.GridSize.X; ++X)
		{
			FIntPoint Coord(X, Y);
			int32 Idx = CoordToIndex(Node.GridSize, Coord);
			if (Node.Cells[Idx].TileType == EPushBoxTileType::Wall) continue;

			for (const FIntPoint& D : Dirs)
			{
				FIntPoint N = Coord + D;
				if (!IsValidCoord(Node.GridSize, N))
				{
					Terrain += 1.0;
				}
				else if (Node.Cells[CoordToIndex(Node.GridSize, N)].TileType == EPushBoxTileType::Wall)
				{
					Terrain += 1.0;
				}
			}
		}
	}
	return Terrain;
}

double UMCTSPuzzleGenerator::ComputeCongestion(const FMCTSNode& Node, const FMCTSConfig& Config) const
{
	double Congestion = 0.0;

	for (int32 i = 0; i < Node.BoxPositions.Num() && i < Node.TargetPositions.Num(); ++i)
	{
		FIntPoint Start = Node.BoxPositions[i];
		FIntPoint Goal = Node.TargetPositions[i];

		int32 MinX = FMath::Min(Start.X, Goal.X);
		int32 MaxX = FMath::Max(Start.X, Goal.X);
		int32 MinY = FMath::Min(Start.Y, Goal.Y);
		int32 MaxY = FMath::Max(Start.Y, Goal.Y);

		int32 BoxCount = 0;
		int32 GoalCount = 0;
		int32 ObstacleCount = 0;

		for (int32 Y = MinY; Y <= MaxY; ++Y)
		{
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				FIntPoint C(X, Y);
				if (C == Start) continue;

				if (Node.BoxPositions.Contains(C)) ++BoxCount;
				if (Node.TargetPositions.Contains(C)) ++GoalCount;

				int32 Idx = CoordToIndex(Node.GridSize, C);
				if (Node.Cells[Idx].TileType == EPushBoxTileType::Wall) ++ObstacleCount;
			}
		}

		Congestion += Config.CongestionBoxWeight * BoxCount
			+ Config.CongestionGoalWeight * GoalCount
			+ Config.CongestionObstacleWeight * ObstacleCount;
	}

	return Congestion;
}

double UMCTSPuzzleGenerator::Evaluate(const FMCTSNode& Node, const FMCTSConfig& Config) const
{
	double Terrain = ComputeTerrain(Node);
	double Congestion = ComputeCongestion(Node, Config);

	double MoveBonus = static_cast<double>(Node.ReverseMoves.Num());

	// Displacement bonus: reward boxes being far from their targets
	double DisplacementSum = 0.0;
	double MinDisplacement = TNumericLimits<double>::Max();
	for (int32 i = 0; i < Node.BoxPositions.Num() && i < Node.TargetPositions.Num(); ++i)
	{
		double Dist = FMath::Abs(Node.BoxPositions[i].X - Node.TargetPositions[i].X)
			+ FMath::Abs(Node.BoxPositions[i].Y - Node.TargetPositions[i].Y);
		DisplacementSum += Dist;
		MinDisplacement = FMath::Min(MinDisplacement, Dist);
	}

	// Clustering bonus: reward boxes being close to each other (like real Sokoban puzzles)
	// Compute average pairwise Manhattan distance between boxes ！ lower = more clustered
	double ClusterBonus = 0.0;
	if (Node.BoxPositions.Num() > 1)
	{
		double TotalPairDist = 0.0;
		int32 PairCount = 0;
		for (int32 i = 0; i < Node.BoxPositions.Num(); ++i)
		{
			for (int32 j = i + 1; j < Node.BoxPositions.Num(); ++j)
			{
				TotalPairDist += FMath::Abs(Node.BoxPositions[i].X - Node.BoxPositions[j].X)
					+ FMath::Abs(Node.BoxPositions[i].Y - Node.BoxPositions[j].Y);
				++PairCount;
			}
		}
		double AvgPairDist = TotalPairDist / PairCount;
		// Invert: smaller avg distance ★ higher bonus. Scale by grid diagonal.
		double MaxDist = Node.GridSize.X + Node.GridSize.Y;
		ClusterBonus = FMath::Max(0.0, MaxDist - AvgPairDist);
	}

	// Distance from centroid of boxes to centroid of targets ！ reward when far apart
	double CentroidDist = 0.0;
	if (Node.BoxPositions.Num() > 0 && Node.TargetPositions.Num() > 0)
	{
		FVector2D BoxCentroid(0, 0);
		for (const FIntPoint& B : Node.BoxPositions)
			BoxCentroid += FVector2D(B.X, B.Y);
		BoxCentroid /= Node.BoxPositions.Num();

		FVector2D TargetCentroid(0, 0);
		for (const FIntPoint& T : Node.TargetPositions)
			TargetCentroid += FVector2D(T.X, T.Y);
		TargetCentroid /= Node.TargetPositions.Num();

		CentroidDist = FMath::Abs(BoxCentroid.X - TargetCentroid.X)
			+ FMath::Abs(BoxCentroid.Y - TargetCentroid.Y);
	}

	double RawScore = FMath::Sqrt(FMath::Max(0.0, Terrain * Congestion));
	RawScore += MoveBonus * 0.5;
	RawScore += DisplacementSum * 0.4;
	RawScore += MinDisplacement * 1.0;
	RawScore += ClusterBonus * 0.3;
	RawScore += CentroidDist * 0.6;

	double Score = RawScore / Config.NormalizationK;
	return FMath::Clamp(Score, 0.0, 1.0);
}

// ============================================================================
// Room Carving
// ============================================================================

void UMCTSPuzzleGenerator::CarveRoom(FMCTSNode& Node, const FMCTSConfig& Config, FRandomStream& Rng)
{
	const int32 SX = Node.GridSize.X;
	const int32 SY = Node.GridSize.Y;

	Node.Cells.SetNum(SX * SY);
	for (FPushBoxCellData& Cell : Node.Cells)
	{
		Cell.TileType = EPushBoxTileType::Wall;
	}

	FIntPoint Center(SX / 2, SY / 2);
	TArray<FIntPoint> Frontier;
	TSet<int32> Carved;

	// Distance from center is used to create irregular falloff at edges.
	// Cells further from center have a higher chance of being skipped,
	// producing an organic, irregular boundary instead of a uniform rectangle.
	const float MaxDist = FMath::Sqrt(static_cast<float>(FMath::Square(SX / 2) + FMath::Square(SY / 2)));

	auto Carve = [&](FIntPoint C)
	{
		int32 Idx = CoordToIndex(Node.GridSize, C);
		if (Carved.Contains(Idx)) return;
		Node.Cells[Idx].TileType = EPushBoxTileType::Empty;
		Carved.Add(Idx);

		static const FIntPoint Dirs[] = { {0,1}, {0,-1}, {1,0}, {-1,0} };
		for (const FIntPoint& D : Dirs)
		{
			FIntPoint N = C + D;
			// Keep a 1-cell wall border at the very edge of the grid
			if (N.X <= 0 || N.X >= SX - 1 || N.Y <= 0 || N.Y >= SY - 1) continue;
			int32 NIdx = CoordToIndex(Node.GridSize, N);
			if (!Carved.Contains(NIdx))
			{
				Frontier.AddUnique(N);
			}
		}
	};

	Carve(Center);

	// Use FloorRatio from config to decide how much interior to carve
	const int32 InteriorCount = (SX - 2) * (SY - 2);
	const int32 TargetCarved = FMath::Clamp(
		FMath::RoundToInt32(InteriorCount * Config.FloorRatio),
		1, InteriorCount);

	while (Carved.Num() < TargetCarved && Frontier.Num() > 0)
	{
		int32 PickIdx = Rng.RandRange(0, Frontier.Num() - 1);
		FIntPoint Next = Frontier[PickIdx];
		Frontier.RemoveAtSwap(PickIdx);

		// Distance-based falloff: cells further from center are more likely to be skipped.
		// This creates an organic, irregular boundary shape.
		float Dist = FMath::Sqrt(static_cast<float>(
			FMath::Square(Next.X - Center.X) + FMath::Square(Next.Y - Center.Y)));
		float NormalizedDist = (MaxDist > 0.f) ? (Dist / MaxDist) : 0.f;

		// Skip probability increases quadratically with distance.
		// At the center the skip chance is ~0, at the max radius it's ~60%.
		float SkipChance = NormalizedDist * NormalizedDist * 0.6f;
		if (Rng.FRand() < SkipChance)
			continue; // leave as wall ！ creates indentations in the boundary

		Carve(Next);
	}

	// Ensure the carved region is fully enclosed by walls (flood-fill sanity check).
	// Any carved cell adjacent to the grid edge that somehow has no wall neighbor
	// would break gameplay, but the 1-cell border guard above prevents this.
}

// ============================================================================
// Box & Player Placement
// ============================================================================

void UMCTSPuzzleGenerator::PlaceBoxesOnTargets(FMCTSNode& Node, int32 NumBoxes, FRandomStream& Rng)
{
	TArray<FIntPoint> FloorCells;
	for (int32 Y = 0; Y < Node.GridSize.Y; ++Y)
	{
		for (int32 X = 0; X < Node.GridSize.X; ++X)
		{
			FIntPoint C(X, Y);
			if (Node.Cells[CoordToIndex(Node.GridSize, C)].TileType == EPushBoxTileType::Empty)
			{
				FloorCells.Add(C);
			}
		}
	}

	for (int32 i = FloorCells.Num() - 1; i > 0; --i)
	{
		int32 j = Rng.RandRange(0, i);
		FloorCells.Swap(i, j);
	}

	int32 Placed = 0;
	for (const FIntPoint& C : FloorCells)
	{
		if (Placed >= NumBoxes) break;

		static const FIntPoint Dirs[] = { {0,1}, {0,-1}, {1,0}, {-1,0} };
		int32 WalkableNeighbors = 0;
		for (const FIntPoint& D : Dirs)
		{
			FIntPoint N = C + D;
			if (IsValidCoord(Node.GridSize, N) &&
				Node.Cells[CoordToIndex(Node.GridSize, N)].TileType != EPushBoxTileType::Wall)
			{
				++WalkableNeighbors;
			}
		}
		if (WalkableNeighbors < 2) continue;

		Node.BoxPositions.Add(C);
		Node.TargetPositions.Add(C);

		int32 Idx = CoordToIndex(Node.GridSize, C);
		Node.Cells[Idx].TileType = EPushBoxTileType::TargetMarker;
		Node.Cells[Idx].bHasBox = true;

		++Placed;
	}
}

void UMCTSPuzzleGenerator::PlacePlayer(FMCTSNode& Node, FRandomStream& Rng)
{
	TArray<FIntPoint> Candidates;
	for (int32 Y = 0; Y < Node.GridSize.Y; ++Y)
	{
		for (int32 X = 0; X < Node.GridSize.X; ++X)
		{
			FIntPoint C(X, Y);
			if (Node.Cells[CoordToIndex(Node.GridSize, C)].TileType != EPushBoxTileType::Empty) continue;
			if (HasBoxAt(Node, C)) continue;
			Candidates.Add(C);
		}
	}

	if (Candidates.Num() > 0)
	{
		Node.PlayerPosition = Candidates[Rng.RandRange(0, Candidates.Num() - 1)];
	}
}

// ============================================================================
// Build Root State
// ============================================================================

TUniquePtr<FMCTSNode> UMCTSPuzzleGenerator::BuildRootState(const FMCTSConfig& Config, FRandomStream& Rng)
{
	auto Root = MakeUnique<FMCTSNode>();
	Root->GridSize = Config.GridSize;
	Root->bInitPhase = false;

	CarveRoom(*Root, Config, Rng);
	PlaceBoxesOnTargets(*Root, Config.NumBoxes, Rng);
	PlacePlayer(*Root, Rng);

	return Root;
}

// ============================================================================
// MCTS Core
// ============================================================================

FMCTSNode* UMCTSPuzzleGenerator::Select(FMCTSNode* Node, float C)
{
	while (Node->Children.Num() > 0 && Node->bFullyExpanded)
	{
		FMCTSNode* BestChild = nullptr;
		double BestUCB = -1.0;

		for (auto& Child : Node->Children)
		{
			double UCB = Child->GetUCBValue(C);
			if (UCB > BestUCB)
			{
				BestUCB = UCB;
				BestChild = Child.Get();
			}
		}

		if (!BestChild) break;
		Node = BestChild;
	}
	return Node;
}

FMCTSNode* UMCTSPuzzleGenerator::Expand(FMCTSNode* Node, FRandomStream& Rng, const FMCTSConfig& Config)
{
	if (Node->UntriedActions.Num() == 0 && Node->Children.Num() == 0)
	{
		TArray<FPullAction> Actions = GetValidPullActions(*Node);
		for (int32 i = 0; i < Actions.Num(); ++i)
		{
			Node->UntriedActions.Add(i);
		}

		if (Actions.Num() == 0)
		{
			Node->bFullyExpanded = true;
			return Node;
		}
	}

	if (Node->UntriedActions.Num() == 0)
	{
		Node->bFullyExpanded = true;
		return Node;
	}

	TArray<FPullAction> Actions = GetValidPullActions(*Node);
	if (Actions.Num() == 0)
	{
		Node->bFullyExpanded = true;
		return Node;
	}

	Node->UntriedActions.RemoveAll([&](int32 Idx) { return Idx >= Actions.Num(); });
	if (Node->UntriedActions.Num() == 0)
	{
		Node->bFullyExpanded = true;
		return Node;
	}

	int32 PickIdx = Rng.RandRange(0, Node->UntriedActions.Num() - 1);
	int32 ActionIdx = Node->UntriedActions[PickIdx];
	Node->UntriedActions.RemoveAtSwap(PickIdx);

	if (Node->UntriedActions.Num() == 0)
	{
		Node->bFullyExpanded = true;
	}

	auto Child = MakeUnique<FMCTSNode>();
	Child->CopyStateFrom(*Node);
	Child->Parent = Node;
	Child->ActionIndex = ActionIdx;

	if (ActionIdx < Actions.Num())
	{
		ApplyReversePull(*Child, Actions[ActionIdx]);
	}

	FMCTSNode* ChildPtr = Child.Get();
	Node->Children.Add(MoveTemp(Child));
	return ChildPtr;
}

double UMCTSPuzzleGenerator::Rollout(const FMCTSNode* Node, const FMCTSConfig& Config, FRandomStream& Rng)
{
	FMCTSNode SimNode;
	SimNode.CopyStateFrom(*Node);

	int32 RemainingPulls = Config.MaxReversePulls - SimNode.ReverseMoves.Num();

	for (int32 i = 0; i < RemainingPulls; ++i)
	{
		TArray<FPullAction> Actions = GetValidPullActions(SimNode);
		if (Actions.Num() == 0) break;

		int32 PickIdx = Rng.RandRange(0, Actions.Num() - 1);
		ApplyReversePull(SimNode, Actions[PickIdx]);
	}

	if (SimNode.ReverseMoves.Num() < Config.MinReversePulls)
	{
		return 0.0;
	}

	return Evaluate(SimNode, Config);
}

void UMCTSPuzzleGenerator::Backpropagate(FMCTSNode* Node, double Score)
{
	while (Node)
	{
		Node->VisitCount++;
		Node->TotalScore += Score;
		Node = Node->Parent;
	}
}

void UMCTSPuzzleGenerator::MCTSIteration(FMCTSNode* Root, const FMCTSConfig& Config, FRandomStream& Rng)
{
	FMCTSNode* Selected = Select(Root, Config.ExplorationConstant);
	FMCTSNode* Expanded = Expand(Selected, Rng, Config);
	double Score = Rollout(Expanded, Config, Rng);
	Backpropagate(Expanded, Score);
}

// ============================================================================
// Critical Path Collection (Ice slides + OneWayDoor + walk paths)
// ============================================================================

void UMCTSPuzzleGenerator::CollectCriticalCells(const FMCTSNode& Node, TSet<int32>& OutCritical) const
{
	// Mark all box positions (start and target)
	for (const FIntPoint& BP : Node.BoxPositions)
		OutCritical.Add(CoordToIndex(Node.GridSize, BP));
	for (const FIntPoint& TP : Node.TargetPositions)
		OutCritical.Add(CoordToIndex(Node.GridSize, TP));

	// Mark player position
	OutCritical.Add(CoordToIndex(Node.GridSize, Node.PlayerPosition));

	// Mark all positions from the move history
	for (const FPushBoxMoveRecord& Move : Node.ReverseMoves)
	{
		OutCritical.Add(CoordToIndex(Node.GridSize, Move.PlayerFrom));
		OutCritical.Add(CoordToIndex(Node.GridSize, Move.PlayerTo));
		OutCritical.Add(CoordToIndex(Node.GridSize, Move.BoxFrom));
		OutCritical.Add(CoordToIndex(Node.GridSize, Move.BoxTo));
	}

	// Mark all player walk paths (the paths the player must walk between pushes)
	for (const TArray<FIntPoint>& WalkPath : Node.PlayerWalkPaths)
	{
		for (const FIntPoint& C : WalkPath)
		{
			OutCritical.Add(CoordToIndex(Node.GridSize, C));
		}
	}

	// Simulate forward to find ice slide intermediate cells and OneWayDoor cells
	// These are part of the solution path too!
	TArray<FPushBoxMoveRecord> ForwardMoves;
	for (int32 i = Node.ReverseMoves.Num() - 1; i >= 0; --i)
	{
		const FPushBoxMoveRecord& Src = Node.ReverseMoves[i];
		FPushBoxMoveRecord Fwd;
		Fwd.Direction = UPushBoxGridManager::GetOppositeDirection(Src.Direction);
		Fwd.PlayerFrom = Src.PlayerTo;
		Fwd.PlayerTo = Src.PlayerFrom;
		Fwd.BoxFrom = Src.BoxTo;
		Fwd.BoxTo = Src.BoxFrom;
		Fwd.bIsReverse = false;
		ForwardMoves.Add(Fwd);
	}

	TArray<FIntPoint> SimBoxes = Node.BoxPositions;
	for (const FPushBoxMoveRecord& Move : ForwardMoves)
	{
		int32 BoxIdx = SimBoxes.IndexOfByKey(Move.BoxFrom);
		if (BoxIdx == INDEX_NONE) continue;

		int32 DestIdx = CoordToIndex(Node.GridSize, Move.BoxTo);
		if (!IsValidCoord(Node.GridSize, Move.BoxTo)) continue;

		// Mark OneWayDoor destination cells
		if (Node.Cells[DestIdx].TileType == EPushBoxTileType::OneWayDoor)
		{
			OutCritical.Add(DestIdx);
		}

		// Trace ice slide path and mark all intermediate cells
		FIntPoint FinalBoxPos = Move.BoxTo;
		if (Node.Cells[DestIdx].TileType == EPushBoxTileType::Ice)
		{
			FIntPoint SlideOff = UPushBoxGridManager::GetDirectionOffset(Move.Direction);
			FIntPoint SlidePos = Move.BoxTo;
			OutCritical.Add(CoordToIndex(Node.GridSize, SlidePos)); // first ice tile

			for (;;)
			{
				FIntPoint Next = SlidePos + SlideOff;
				if (!IsValidCoord(Node.GridSize, Next)) break;
				int32 NIdx = CoordToIndex(Node.GridSize, Next);
				if (Node.Cells[NIdx].TileType == EPushBoxTileType::Wall) break;
				if (SimBoxes.Contains(Next) && Next != Move.BoxFrom) break;
				if (Node.Cells[NIdx].TileType == EPushBoxTileType::OneWayDoor &&
					Node.Cells[NIdx].OneWayDirection != Move.Direction) break;
				SlidePos = Next;
				OutCritical.Add(CoordToIndex(Node.GridSize, SlidePos)); // intermediate ice tiles
				if (Node.Cells[CoordToIndex(Node.GridSize, SlidePos)].TileType != EPushBoxTileType::Ice) break;
			}
			FinalBoxPos = SlidePos;
		}

		SimBoxes[BoxIdx] = FinalBoxPos;
	}
}

// ============================================================================
// Post-Processing: Obstacle Placement
// ============================================================================

void UMCTSPuzzleGenerator::PostProcessObstacles(FMCTSNode& Node, const FMCTSConfig& Config, FRandomStream& Rng)
{
	TSet<int32> CriticalCells;
	CollectCriticalCells(Node, CriticalCells);

	// Mark maneuvering cells (neighbors of critical path)
	TSet<int32> ManeuveringCells;
	static const FIntPoint Dirs[] = { {0,1}, {0,-1}, {1,0}, {-1,0} };
	for (int32 CIdx : CriticalCells)
	{
		FIntPoint C(CIdx % Node.GridSize.X, CIdx / Node.GridSize.X);
		for (const FIntPoint& D : Dirs)
		{
			FIntPoint N = C + D;
			if (!IsValidCoord(Node.GridSize, N)) continue;
			int32 NIdx = CoordToIndex(Node.GridSize, N);
			if (!CriticalCells.Contains(NIdx))
				ManeuveringCells.Add(NIdx);
		}
	}

	// Gather unrestricted cells (not critical, not a box, not already wall)
	TArray<FIntPoint> UnrestrictedCells;
	for (int32 Y = 0; Y < Node.GridSize.Y; ++Y)
	{
		for (int32 X = 0; X < Node.GridSize.X; ++X)
		{
			FIntPoint C(X, Y);
			int32 Idx = CoordToIndex(Node.GridSize, C);
			if (Node.Cells[Idx].TileType != EPushBoxTileType::Empty) continue;
			if (CriticalCells.Contains(Idx)) continue;
			if (HasBoxAt(Node, C)) continue;
			UnrestrictedCells.Add(C);
		}
	}

	// Shuffle
	for (int32 i = UnrestrictedCells.Num() - 1; i > 0; --i)
	{
		int32 j = Rng.RandRange(0, i);
		UnrestrictedCells.Swap(i, j);
	}

	static const EPushBoxDirection AllDirsArr[] = {
		EPushBoxDirection::North, EPushBoxDirection::East,
		EPushBoxDirection::South, EPushBoxDirection::West
	};

	for (const FIntPoint& C : UnrestrictedCells)
	{
		int32 Idx = CoordToIndex(Node.GridSize, C);
		bool bIsManeuvering = ManeuveringCells.Contains(Idx);

		float Roll = Rng.FRand();
		float CumulWall = Config.WallDensity;
		float CumulIce = CumulWall + Config.IceDensity;
		float CumulDoor = CumulIce + Config.OneWayDoorDensity;

		EPushBoxTileType NewType = EPushBoxTileType::Empty;

		if (!bIsManeuvering && Roll < CumulWall)
		{
			NewType = EPushBoxTileType::Wall;
		}
		else if (Roll < CumulIce)
		{
			NewType = EPushBoxTileType::Ice;
		}
		else if (Roll < CumulDoor)
		{
			NewType = EPushBoxTileType::OneWayDoor;
		}

		if (NewType == EPushBoxTileType::Empty) continue;

		EPushBoxTileType OldType = Node.Cells[Idx].TileType;
		EPushBoxDirection OldDir = Node.Cells[Idx].OneWayDirection;

		if (NewType == EPushBoxTileType::OneWayDoor)
		{
			// Try all 4 directions in shuffled order to find one that doesn't break the solution
			TArray<int32> DirOrder = { 0, 1, 2, 3 };
			for (int32 i = 3; i > 0; --i)
			{
				int32 j = Rng.RandRange(0, i);
				DirOrder.Swap(i, j);
			}

			bool bPlaced = false;
			for (int32 di : DirOrder)
			{
				Node.Cells[Idx].TileType = EPushBoxTileType::OneWayDoor;
				Node.Cells[Idx].OneWayDirection = AllDirsArr[di];

				if (ValidateForward(Node))
				{
					bPlaced = true;
					break;
				}
			}

			if (!bPlaced)
			{
				// No direction works ！ revert
				Node.Cells[Idx].TileType = OldType;
				Node.Cells[Idx].OneWayDirection = OldDir;
			}
		}
		else
		{
			// Wall or Ice ！ simple tentative place + validate
			Node.Cells[Idx].TileType = NewType;
			Node.Cells[Idx].OneWayDirection = EPushBoxDirection::None;

			if (!ValidateForward(Node))
			{
				Node.Cells[Idx].TileType = OldType;
				Node.Cells[Idx].OneWayDirection = OldDir;
			}
		}
	}
}

// ============================================================================
// Post-Processing: Place Ice/Doors on Critical Path
// ============================================================================

void UMCTSPuzzleGenerator::PostProcessCriticalPathSpecials(FMCTSNode& Node, const FMCTSConfig& Config, FRandomStream& Rng)
{
	// Collect critical path cells ！ these are cells the box actually passes through
	TSet<int32> CriticalCells;
	CollectCriticalCells(Node, CriticalCells);

	// Identify cells where a box moves THROUGH (not start/end positions, not player-only cells)
	// These are the best candidates for ice (box slides through) or doors (box enters directionally)
	TArray<FIntPoint> BoxPathCells;
	for (const FPushBoxMoveRecord& Move : Node.ReverseMoves)
	{
		// In the forward direction, the box moves from BoxTo ★ BoxFrom (since these are reverse records)
		// We want the forward box destination cells
		FIntPoint FwdBoxTo = Move.BoxFrom; // reverse's BoxFrom = forward's BoxTo
		int32 FwdIdx = CoordToIndex(Node.GridSize, FwdBoxTo);

		// Only consider empty floor cells that are on the critical path
		if (!IsValidCoord(Node.GridSize, FwdBoxTo)) continue;
		if (Node.Cells[FwdIdx].TileType != EPushBoxTileType::Empty) continue;

		// Don't place specials on target or box-start positions
		if (Node.TargetPositions.Contains(FwdBoxTo)) continue;
		if (Node.BoxPositions.Contains(FwdBoxTo)) continue;

		BoxPathCells.AddUnique(FwdBoxTo);
	}

	// Also consider forward walk-path cells for player-oriented doors
	// (player must walk through a one-way door to reach push position)
	TArray<FIntPoint> WalkPathCells;
	for (const TArray<FIntPoint>& WalkPath : Node.PlayerWalkPaths)
	{
		for (const FIntPoint& C : WalkPath)
		{
			int32 Idx = CoordToIndex(Node.GridSize, C);
			if (Node.Cells[Idx].TileType != EPushBoxTileType::Empty) continue;
			if (Node.TargetPositions.Contains(C)) continue;
			if (Node.BoxPositions.Contains(C)) continue;
			if (HasBoxAt(Node, C)) continue;
			WalkPathCells.AddUnique(C);
		}
	}

	// Shuffle both lists
	for (int32 i = BoxPathCells.Num() - 1; i > 0; --i)
		BoxPathCells.Swap(i, Rng.RandRange(0, i));
	for (int32 i = WalkPathCells.Num() - 1; i > 0; --i)
		WalkPathCells.Swap(i, Rng.RandRange(0, i));

	static const EPushBoxDirection AllDirsArr[] = {
		EPushBoxDirection::North, EPushBoxDirection::East,
		EPushBoxDirection::South, EPushBoxDirection::West
	};

	// Try placing ice on box path cells
	int32 IcePlaced = 0;
	int32 MaxIce = FMath::Max(1, BoxPathCells.Num() / 3);
	for (const FIntPoint& C : BoxPathCells)
	{
		if (IcePlaced >= MaxIce) break;
		if (Rng.FRand() > 0.5f) continue; // 50% chance per candidate

		int32 Idx = CoordToIndex(Node.GridSize, C);
		EPushBoxTileType OldType = Node.Cells[Idx].TileType;

		Node.Cells[Idx].TileType = EPushBoxTileType::Ice;

		if (ValidateForward(Node))
		{
			++IcePlaced;
		}
		else
		{
			Node.Cells[Idx].TileType = OldType;
		}
	}

	// Try placing one-way doors on walk path cells and box path cells
	TArray<FIntPoint> DoorCandidates;
	DoorCandidates.Append(WalkPathCells);
	DoorCandidates.Append(BoxPathCells);
	for (int32 i = DoorCandidates.Num() - 1; i > 0; --i)
		DoorCandidates.Swap(i, Rng.RandRange(0, i));

	int32 DoorsPlaced = 0;
	int32 MaxDoors = FMath::Max(1, DoorCandidates.Num() / 4);
	for (const FIntPoint& C : DoorCandidates)
	{
		if (DoorsPlaced >= MaxDoors) break;
		if (Rng.FRand() > 0.4f) continue; // 40% chance per candidate

		int32 Idx = CoordToIndex(Node.GridSize, C);
		if (Node.Cells[Idx].TileType != EPushBoxTileType::Empty) continue; // may have been turned to ice above

		EPushBoxTileType OldType = Node.Cells[Idx].TileType;
		EPushBoxDirection OldDir = Node.Cells[Idx].OneWayDirection;

		// Try all 4 directions
		TArray<int32> DirOrder = { 0, 1, 2, 3 };
		for (int32 di = 3; di > 0; --di)
			DirOrder.Swap(di, Rng.RandRange(0, di));

		bool bPlaced = false;
		for (int32 di : DirOrder)
		{
			Node.Cells[Idx].TileType = EPushBoxTileType::OneWayDoor;
			Node.Cells[Idx].OneWayDirection = AllDirsArr[di];

			if (ValidateForward(Node))
			{
				bPlaced = true;
				++DoorsPlaced;
				break;
			}
		}

		if (!bPlaced)
		{
			Node.Cells[Idx].TileType = OldType;
			Node.Cells[Idx].OneWayDirection = OldDir;
		}
	}

	UE_LOG(LogMCTS, Log, TEXT("CriticalPathSpecials: Placed %d ice, %d doors on solution path"), IcePlaced, DoorsPlaced);
}

// ============================================================================
// Forward Validation
// ============================================================================

bool UMCTSPuzzleGenerator::ValidateForward(const FMCTSNode& Node) const
{
	if (Node.ReverseMoves.Num() == 0) return false;

	// Build forward moves from reverse moves
	TArray<FPushBoxMoveRecord> ForwardMoves;
	ForwardMoves.Reserve(Node.ReverseMoves.Num());
	for (int32 i = Node.ReverseMoves.Num() - 1; i >= 0; --i)
	{
		const FPushBoxMoveRecord& Src = Node.ReverseMoves[i];
		FPushBoxMoveRecord Fwd;
		Fwd.Direction = UPushBoxGridManager::GetOppositeDirection(Src.Direction);
		Fwd.PlayerFrom = Src.PlayerTo;
		Fwd.PlayerTo = Src.PlayerFrom;
		Fwd.BoxFrom = Src.BoxTo;
		Fwd.BoxTo = Src.BoxFrom;
		Fwd.bIsReverse = false;
		ForwardMoves.Add(Fwd);
	}

	// Simulate forward from the start state
	TArray<FIntPoint> SimBoxes = Node.BoxPositions;
	FIntPoint SimPlayer = Node.PlayerPosition;

	for (int32 MoveIdx = 0; MoveIdx < ForwardMoves.Num(); ++MoveIdx)
	{
		const FPushBoxMoveRecord& Move = ForwardMoves[MoveIdx];

		// The push position = the cell opposite to the push direction, adjacent to the box
		FIntPoint PushPos = Move.BoxFrom + UPushBoxGridManager::GetDirectionOffset(
			UPushBoxGridManager::GetOppositeDirection(Move.Direction));

		// Build a temp node with current simulation state for BFS
		FMCTSNode TempNode;
		TempNode.Cells = Node.Cells;
		TempNode.GridSize = Node.GridSize;
		TempNode.BoxPositions = SimBoxes;
		TempNode.PlayerPosition = SimPlayer;

		// Verify player can walk to push position
		if (!CanPlayerReach(TempNode, SimPlayer, PushPos))
			return false;

		// Verify destination cell is valid
		if (!IsValidCoord(Node.GridSize, Move.BoxTo)) return false;
		int32 DestIdx = CoordToIndex(Node.GridSize, Move.BoxTo);
		if (Node.Cells[DestIdx].TileType == EPushBoxTileType::Wall) return false;

		// Check one-way door direction for box entry
		if (Node.Cells[DestIdx].TileType == EPushBoxTileType::OneWayDoor)
		{
			if (Node.Cells[DestIdx].OneWayDirection != Move.Direction) return false;
		}

		// Check not pushing onto another box
		for (const FIntPoint& BP : SimBoxes)
		{
			if (BP == Move.BoxTo && BP != Move.BoxFrom) return false;
		}

		// Find and move the box
		int32 BoxIdx = SimBoxes.IndexOfByKey(Move.BoxFrom);
		if (BoxIdx == INDEX_NONE) return false;

		// Handle ice sliding
		FIntPoint FinalBoxPos = Move.BoxTo;
		if (Node.Cells[DestIdx].TileType == EPushBoxTileType::Ice)
		{
			FIntPoint SlideOff = UPushBoxGridManager::GetDirectionOffset(Move.Direction);
			FIntPoint SlidePos = Move.BoxTo;
			for (;;)
			{
				FIntPoint Next = SlidePos + SlideOff;
				if (!IsValidCoord(Node.GridSize, Next)) break;
				int32 NIdx = CoordToIndex(Node.GridSize, Next);
				if (Node.Cells[NIdx].TileType == EPushBoxTileType::Wall) break;
				if (SimBoxes.Contains(Next) && Next != Move.BoxFrom) break;
				if (Node.Cells[NIdx].TileType == EPushBoxTileType::OneWayDoor &&
					Node.Cells[NIdx].OneWayDirection != Move.Direction) break;
				SlidePos = Next;
				if (Node.Cells[CoordToIndex(Node.GridSize, SlidePos)].TileType != EPushBoxTileType::Ice) break;
			}
			FinalBoxPos = SlidePos;
		}

		SimBoxes[BoxIdx] = FinalBoxPos;
		// After pushing, the player is at the box's old position
		SimPlayer = Move.BoxFrom;
	}

	// Check all boxes are on targets
	for (const FIntPoint& Target : Node.TargetPositions)
	{
		if (!SimBoxes.Contains(Target)) return false;
	}

	return true;
}

// ============================================================================
// Main Generate Function
// ============================================================================

FMCTSResult UMCTSPuzzleGenerator::Generate(const FMCTSConfig& Config)
{
	FMCTSResult Result;
	Result.bSuccess = false;

	FRandomStream Rng(Config.Seed != 0 ? Config.Seed : FMath::Rand());

	UE_LOG(LogMCTS, Log, TEXT("MCTS Generation starting: Grid=%dx%d, Boxes=%d, TimeBudget=%.1fs"),
		Config.GridSize.X, Config.GridSize.Y, Config.NumBoxes, Config.TimeBudgetSeconds);

	double BestScore = -1.0;
	FMCTSNode BestPuzzle;

	const double StartTime = FPlatformTime::Seconds();
	const double EndTime = StartTime + Config.TimeBudgetSeconds;
	int32 TotalSimulations = 0;
	int32 TreeCount = 0;

	while (FPlatformTime::Seconds() < EndTime)
	{
		TUniquePtr<FMCTSNode> Root = BuildRootState(Config, Rng);
		++TreeCount;

		TArray<FPullAction> RootActions = GetValidPullActions(*Root);
		if (RootActions.Num() == 0)
		{
			continue;
		}

		const double TreeTimeLimit = FMath::Min(
			FPlatformTime::Seconds() + Config.TimeBudgetSeconds * 0.3,
			EndTime);

		while (FPlatformTime::Seconds() < TreeTimeLimit)
		{
			MCTSIteration(Root.Get(), Config, Rng);
			++TotalSimulations;

			if (Config.MaxSimulations > 0 && TotalSimulations >= Config.MaxSimulations)
				break;
		}

		// Find best leaf in this tree ！ must pass displacement + forward validation
		TArray<FMCTSNode*> LeafQueue;
		LeafQueue.Add(Root.Get());
		while (LeafQueue.Num() > 0)
		{
			FMCTSNode* Current = LeafQueue[0];
			LeafQueue.RemoveAt(0, EAllowShrinking::No);

			for (auto& Child : Current->Children)
			{
				LeafQueue.Add(Child.Get());
			}

			if (Current->Children.Num() != 0) continue;
			if (Current->ReverseMoves.Num() < Config.MinReversePulls) continue;

			// Check minimum box displacement ！ no box may start on/near its target
			bool bDisplacementOK = true;
			for (int32 i = 0; i < Current->BoxPositions.Num() && i < Current->TargetPositions.Num(); ++i)
			{
				int32 Dist = FMath::Abs(Current->BoxPositions[i].X - Current->TargetPositions[i].X)
					+ FMath::Abs(Current->BoxPositions[i].Y - Current->TargetPositions[i].Y);
				if (Dist < Config.MinBoxDisplacement)
				{
					bDisplacementOK = false;
					break;
				}
			}
			if (!bDisplacementOK) continue;

			double Score = Evaluate(*Current, Config);
			if (Score <= BestScore) continue;

			if (ValidateForward(*Current))
			{
				BestScore = Score;
				BestPuzzle.CopyStateFrom(*Current);
				UE_LOG(LogMCTS, Log, TEXT("Tree %d: New best score=%.4f (moves=%d, sims=%d, VALIDATED)"),
					TreeCount, BestScore, Current->ReverseMoves.Num(), TotalSimulations);
			}
		}

		if (Config.MaxSimulations > 0 && TotalSimulations >= Config.MaxSimulations)
			break;
	}

	const double Elapsed = FPlatformTime::Seconds() - StartTime;
	Result.ElapsedSeconds = static_cast<float>(Elapsed);
	Result.SimulationCount = TotalSimulations;

	if (BestScore <= 0.0 || BestPuzzle.ReverseMoves.Num() < Config.MinReversePulls)
	{
		UE_LOG(LogMCTS, Warning, TEXT("MCTS: Failed to find a valid puzzle (best score=%.4f, %d sims in %.2fs)"),
			BestScore, TotalSimulations, Elapsed);
		return Result;
	}

	// Post-process: add obstacles
	PostProcessObstacles(BestPuzzle, Config, Rng);

	// Post-process: place ice/doors on the critical path to make them part of the solution
	PostProcessCriticalPathSpecials(BestPuzzle, Config, Rng);

	// Build result
	Result.bSuccess = true;
	Result.BestScore = static_cast<float>(BestScore);
	Result.ReverseMoves = BestPuzzle.ReverseMoves;

	// Forward solution
	Result.ForwardSolution.Reserve(BestPuzzle.ReverseMoves.Num());
	for (int32 i = BestPuzzle.ReverseMoves.Num() - 1; i >= 0; --i)
	{
		const FPushBoxMoveRecord& Src = BestPuzzle.ReverseMoves[i];
		FPushBoxMoveRecord Fwd;
		Fwd.Direction = UPushBoxGridManager::GetOppositeDirection(Src.Direction);
		Fwd.PlayerFrom = Src.PlayerTo;
		Fwd.PlayerTo = Src.PlayerFrom;
		Fwd.BoxFrom = Src.BoxTo;
		Fwd.BoxTo = Src.BoxFrom;
		Fwd.bIsReverse = false;
		Result.ForwardSolution.Add(Fwd);
	}

	// Build goal state snapshot
	Result.GoalState.GridSize = BestPuzzle.GridSize;
	Result.GoalState.Cells = BestPuzzle.Cells;
	Result.GoalState.PlayerPosition = BestPuzzle.PlayerPosition;
	Result.GoalState.BoxPositions = BestPuzzle.TargetPositions;
	for (int32 i = 0; i < BestPuzzle.Cells.Num(); ++i)
	{
		Result.GoalState.Cells[i].bHasBox = false;
	}
	for (const FIntPoint& TP : BestPuzzle.TargetPositions)
	{
		int32 Idx = CoordToIndex(BestPuzzle.GridSize, TP);
		Result.GoalState.Cells[Idx].TileType = EPushBoxTileType::TargetMarker;
	}

	// Build start state snapshot
	Result.StartState.GridSize = BestPuzzle.GridSize;
	Result.StartState.Cells = BestPuzzle.Cells;
	Result.StartState.PlayerPosition = BestPuzzle.PlayerPosition;
	Result.StartState.BoxPositions = BestPuzzle.BoxPositions;
	for (int32 i = 0; i < BestPuzzle.Cells.Num(); ++i)
	{
		Result.StartState.Cells[i].bHasBox = false;
	}
	for (const FIntPoint& TP : BestPuzzle.TargetPositions)
	{
		int32 Idx = CoordToIndex(BestPuzzle.GridSize, TP);
		Result.StartState.Cells[Idx].TileType = EPushBoxTileType::TargetMarker;
	}

	UE_LOG(LogMCTS, Log, TEXT("MCTS: Generated puzzle! Score=%.4f, Moves=%d, Sims=%d, Trees=%d, Time=%.2fs"),
		Result.BestScore, Result.ForwardSolution.Num(), TotalSimulations, TreeCount, Elapsed);

	return Result;
}
