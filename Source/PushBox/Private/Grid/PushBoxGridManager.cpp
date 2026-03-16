// Copyright PushBox Game. All Rights Reserved.

#include "Grid/PushBoxGridManager.h"
#include "Data/PuzzleLevelDataAsset.h"
#include "PushBoxTypes.h"
#include "WFCGeneratorComponent.h"
#include "WFCAsset.h"
#include "Core/Grids/WFCGrid2D.h"
#include "GameFramework/GameplayMessageSubsystem.h"

namespace
{
	UGameplayMessageSubsystem* GetMsgSubsystem(const UObject* WorldContext)
	{
		if (!WorldContext) return nullptr;
		UWorld* World = WorldContext->GetWorld();
		if (!World) return nullptr;
		UGameInstance* GI = World->GetGameInstance();
		return GI ? GI->GetSubsystem<UGameplayMessageSubsystem>() : nullptr;
	}
}

// ==================================================================
// Construction / BeginPlay
// ==================================================================

UPushBoxGridManager::UPushBoxGridManager()
{
	PrimaryComponentTick.bCanEverTick = false;
	CurrentPuzzleState = EPuzzleState::NotStarted;
	MoveCount = 0;
	GridSize = FIntPoint::ZeroValue;
}

void UPushBoxGridManager::BeginPlay()
{
	Super::BeginPlay();
	SyncWithWFCGenerator();
	if (PuzzleData)
	{
		InitializeFromDataAsset(PuzzleData);
	}
}

// ==================================================================
// Initialization
// ==================================================================

void UPushBoxGridManager::InitializeFromDataAsset(UPuzzleLevelDataAsset* InPuzzleData)
{
	if (!InPuzzleData)
	{
		UE_LOG(LogPushBox, Error, TEXT("InitializeFromDataAsset: null PuzzleData"));
		return;
	}
	PuzzleData = InPuzzleData;
	GridSize = InPuzzleData->GridSize;
	PlayerGridPosition = InPuzzleData->StartState.PlayerPosition;
	BoxPositions = InPuzzleData->StartState.BoxPositions;
	MoveHistory.Empty();
	MoveCount = 0;

	// Load cells from data asset ！ if missing, create a blank grid with border walls
	const int32 ExpectedSize = GridSize.X * GridSize.Y;
	if (InPuzzleData->StartState.Cells.Num() == ExpectedSize)
	{
		Cells = InPuzzleData->StartState.Cells;
	}
	else
	{
		UE_LOG(LogPushBox, Warning, TEXT("InitializeFromDataAsset: Cells array size %d != expected %d, creating blank cells"),
			InPuzzleData->StartState.Cells.Num(), ExpectedSize);
		Cells.SetNum(ExpectedSize);
		for (int32 Y = 0; Y < GridSize.Y; ++Y)
		{
			for (int32 X = 0; X < GridSize.X; ++X)
			{
				FPushBoxCellData& Cell = Cells[Y * GridSize.X + X];
				const bool bBorder = (X == 0 || Y == 0 || X == GridSize.X - 1 || Y == GridSize.Y - 1);
				Cell.TileType = bBorder ? EPushBoxTileType::Wall : EPushBoxTileType::Empty;
				Cell.bHasBox = false;
			}
		}
	}

	SetPuzzleState(EPuzzleState::InProgress);
}

void UPushBoxGridManager::InitializeBlankGrid(FIntPoint InGridSize)
{
	GridSize = InGridSize;
	Cells.SetNum(GridSize.X * GridSize.Y);
	for (int32 Y = 0; Y < GridSize.Y; ++Y)
	{
		for (int32 X = 0; X < GridSize.X; ++X)
		{
			FPushBoxCellData& Cell = Cells[Y * GridSize.X + X];
			const bool bBorder = (X == 0 || Y == 0 || X == GridSize.X - 1 || Y == GridSize.Y - 1);
			Cell.TileType = bBorder ? EPushBoxTileType::Wall : EPushBoxTileType::Empty;
			Cell.bHasBox = false;
		}
	}
	BoxPositions.Empty();
	PlayerGridPosition = FIntPoint(1, 1);
	MoveHistory.Empty();
	MoveCount = 0;
	SetPuzzleState(EPuzzleState::NotStarted);
}

void UPushBoxGridManager::SyncWithWFCGenerator()
{
	if (!GetOwner()) return;
	CachedWFCGenerator = GetOwner()->FindComponentByClass<UWFCGeneratorComponent>();
	if (!CachedWFCGenerator) return;

	// First try the live grid (available after WFC generation)
	const UWFCGrid* Grid = CachedWFCGenerator->GetGrid();
	if (Grid)
	{
		const UWFCGrid2D* Grid2D = Cast<UWFCGrid2D>(Grid);
		if (Grid2D)
		{
			GridSize = Grid2D->Dimensions;
			CellSize = Grid2D->CellSize;
			return;
		}
	}

	// Fallback: read from the WFCAsset's GridConfig (before generation runs)
	if (CachedWFCGenerator->WFCAsset && CachedWFCGenerator->WFCAsset->GridConfig)
	{
		const UWFCGrid2DConfig* Config2D = Cast<UWFCGrid2DConfig>(CachedWFCGenerator->WFCAsset->GridConfig);
		if (Config2D)
		{
			GridSize = Config2D->Dimensions;
			CellSize = Config2D->CellSize;
		}
	}
}

// ==================================================================
// Gameplay  Forward (player pushes boxes)
// ==================================================================

bool UPushBoxGridManager::TryMovePlayer(EPushBoxDirection Direction)
{
	if (CurrentPuzzleState != EPuzzleState::InProgress)
		return false;

	if (Cells.Num() == 0)
	{
		UE_LOG(LogPushBox, Warning, TEXT("TryMovePlayer: Cells array is empty, grid not initialized"));
		return false;
	}

	const FIntPoint Offset = GetDirectionOffset(Direction);
	const FIntPoint NewPlayerPos = PlayerGridPosition + Offset;

	if (!IsValidCoord(NewPlayerPos))
		return false;

	const int32 TargetIdx = CoordToIndex(NewPlayerPos);
	if (!Cells.IsValidIndex(TargetIdx))
		return false;

	// Player is blocked ONLY by Wall and by boxes.
	const FPushBoxCellData& TargetCell = Cells[TargetIdx];
	if (TargetCell.TileType == EPushBoxTileType::Wall)
		return false;

	if (HasBoxAt(NewPlayerPos))
	{
		// --- push the box ---
		const FIntPoint NewBoxPos = NewPlayerPos + Offset;
		if (!IsValidCoord(NewBoxPos))
			return false;
		if (!CanBoxEnterCell(NewBoxPos, Direction))
			return false;

		int32 BoxIdx = BoxPositions.IndexOfByKey(NewPlayerPos);
		if (BoxIdx == INDEX_NONE)
			return false;

		FIntPoint FinalBoxPos = NewBoxPos;
		const FPushBoxCellData& BoxDest = Cells[CoordToIndex(NewBoxPos)];

		// Ice: box slides until it hits something
		if (BoxDest.TileType == EPushBoxTileType::Ice)
		{
			FIntPoint SlidePos = NewBoxPos;
			for (;;)
			{
				FIntPoint Next = SlidePos + Offset;
				if (!IsValidCoord(Next))
					break;
				const FPushBoxCellData& NC = Cells[CoordToIndex(Next)];
				if (NC.TileType == EPushBoxTileType::Wall || HasBoxAt(Next))
					break;
				if (NC.TileType == EPushBoxTileType::OneWayDoor && NC.OneWayDirection != Direction)
					break;
				SlidePos = Next;
				if (Cells[CoordToIndex(SlidePos)].TileType != EPushBoxTileType::Ice)
					break;
			}
			FinalBoxPos = SlidePos;
		}

		BoxPositions[BoxIdx] = FinalBoxPos;
		BroadcastBoxMoved(NewPlayerPos, FinalBoxPos, Direction);

		FPushBoxMoveRecord Rec;
		Rec.Direction   = Direction;
		Rec.PlayerFrom  = PlayerGridPosition;
		Rec.PlayerTo    = NewPlayerPos;
		Rec.BoxFrom     = NewPlayerPos;
		Rec.BoxTo       = FinalBoxPos;
		Rec.bIsReverse  = false;
		MoveHistory.Add(Rec);
	}
	else
	{
		// --- simple walk ---
		FPushBoxMoveRecord Rec;
		Rec.Direction   = Direction;
		Rec.PlayerFrom  = PlayerGridPosition;
		Rec.PlayerTo    = NewPlayerPos;
		Rec.BoxFrom     = FIntPoint(-1, -1);
		Rec.BoxTo       = FIntPoint(-1, -1);
		Rec.bIsReverse  = false;
		MoveHistory.Add(Rec);
	}

	const FIntPoint OldPos = PlayerGridPosition;
	PlayerGridPosition = NewPlayerPos;
	++MoveCount;
	BroadcastPlayerMoved(OldPos, NewPlayerPos, Direction);

	if (CheckWinCondition())
	{
		SetPuzzleState(EPuzzleState::Completed);
	}
	return true;
}

bool UPushBoxGridManager::UndoLastMove()
{
	if (MoveHistory.Num() == 0)
		return false;

	const FPushBoxMoveRecord Last = MoveHistory.Last();
	PlayerGridPosition = Last.PlayerFrom;

	if (Last.BoxFrom != FIntPoint(-1, -1))
	{
		int32 BoxIdx = BoxPositions.IndexOfByKey(Last.BoxTo);
		if (BoxIdx != INDEX_NONE)
		{
			BoxPositions[BoxIdx] = Last.BoxFrom;
		}
	}

	MoveHistory.RemoveAt(MoveHistory.Num() - 1);
	MoveCount = FMath::Max(0, MoveCount - 1);

	if (CurrentPuzzleState == EPuzzleState::Completed)
	{
		SetPuzzleState(EPuzzleState::InProgress);
	}

	if (UGameplayMessageSubsystem* Msg = GetMsgSubsystem(this))
	{
		FPushBoxMessage_PlayerMoved M;
		M.FromCoord = Last.PlayerTo;
		M.ToCoord   = Last.PlayerFrom;
		M.Direction = GetOppositeDirection(Last.Direction);
		Msg->BroadcastMessage(PushBoxTags::Message_UndoPerformed, M);
	}
	return true;
}

void UPushBoxGridManager::ResetToStart()
{
	if (PuzzleData)
	{
		InitializeFromDataAsset(PuzzleData);
	}
}

// ==================================================================
// Gameplay  Reverse (editor pulls boxes backward)
// ==================================================================

bool UPushBoxGridManager::TryReversePullBox(FIntPoint BoxCoord, EPushBoxDirection PullDir)
{
	if (!HasBoxAt(BoxCoord))
		return false;

	const FIntPoint Off            = GetDirectionOffset(PullDir);
	// The player stands on the pull-side of the box, then both move in PullDir.
	// PlayerPos = where the player must be standing (adjacent to box in PullDir)
	// PlayerNewPos = where the player ends up (one more step in PullDir)
	// BoxNewPos = where the box ends up initially (the old PlayerPos)
	const FIntPoint PlayerPos      = BoxCoord + Off;
	const FIntPoint PlayerNewPos   = PlayerPos + Off;
	const FIntPoint BoxNewPos      = BoxCoord + Off;   // = PlayerPos

	if (!IsValidCoord(PlayerPos) || !IsValidCoord(PlayerNewPos) || !IsValidCoord(BoxNewPos))
		return false;

	// Player movement: only blocked by walls
	if (Cells[CoordToIndex(PlayerNewPos)].TileType == EPushBoxTileType::Wall)
		return false;
	if (Cells[CoordToIndex(PlayerPos)].TileType == EPushBoxTileType::Wall)
		return false;

	// The box destination (BoxNewPos == PlayerPos) must not be a wall
	// and must not have another box already there.
	// NOTE: In reverse mode, OneWayDoor does NOT block box entry from any direction.
	// (Forward: only one direction allowed. Reverse = undo of forward, so any direction is valid.)
	if (Cells[CoordToIndex(BoxNewPos)].TileType == EPushBoxTileType::Wall)
		return false;
	if (HasBoxAt(BoxNewPos))
		return false;

	int32 BoxIdx = BoxPositions.IndexOfByKey(BoxCoord);
	if (BoxIdx == INDEX_NONE)
		return false;

	// Ice sliding: if the box lands on ice, it slides in the pull direction
	// until it reaches a non-ice cell or is blocked.
	// This mirrors the forward push ice logic ！ push/pull on ice is fully reversible.
	FIntPoint FinalBoxPos = BoxNewPos;
	const FPushBoxCellData& DestCell = Cells[CoordToIndex(BoxNewPos)];
	if (DestCell.TileType == EPushBoxTileType::Ice)
	{
		FIntPoint SlidePos = BoxNewPos;
		for (;;)
		{
			FIntPoint Next = SlidePos + Off;
			if (!IsValidCoord(Next))
				break;
			const FPushBoxCellData& NC = Cells[CoordToIndex(Next)];
			if (NC.TileType == EPushBoxTileType::Wall || HasBoxAt(Next))
				break;
			// In reverse, OneWayDoor does not block sliding
			SlidePos = Next;
			if (Cells[CoordToIndex(SlidePos)].TileType != EPushBoxTileType::Ice)
				break;
		}
		FinalBoxPos = SlidePos;
	}

	BoxPositions[BoxIdx] = FinalBoxPos;

	FPushBoxMoveRecord Rec;
	Rec.Direction   = PullDir;
	Rec.PlayerFrom  = PlayerPos;
	Rec.PlayerTo    = PlayerNewPos;
	Rec.BoxFrom     = BoxCoord;
	Rec.BoxTo       = FinalBoxPos;
	Rec.bIsReverse  = true;
	MoveHistory.Add(Rec);

	PlayerGridPosition = PlayerNewPos;
	++MoveCount;
	OnBoxMoved.Broadcast(BoxCoord, FinalBoxPos);
	return true;
}

bool UPushBoxGridManager::TryReversePlayerMove(EPushBoxDirection Direction)
{
	const FIntPoint Off = GetDirectionOffset(Direction);
	const FIntPoint NewPos = PlayerGridPosition + Off;

	if (!IsValidCoord(NewPos))
		return false;

	// In reverse mode, the player is only blocked by walls.
	// Ice, OneWayDoor, and even boxes do NOT block the player's own movement.
	// (OneWayDoor and Ice only affect box physics, not the player character.)
	const FPushBoxCellData& Cell = Cells[CoordToIndex(NewPos)];
	if (Cell.TileType == EPushBoxTileType::Wall)
		return false;

	FPushBoxMoveRecord Rec;
	Rec.Direction = Direction;
	Rec.PlayerFrom = PlayerGridPosition;
	Rec.PlayerTo = NewPos;
	Rec.BoxFrom = FIntPoint(-1, -1);
	Rec.BoxTo = FIntPoint(-1, -1);
	Rec.bIsReverse = true;
	MoveHistory.Add(Rec);

	PlayerGridPosition = NewPos;
	++MoveCount;
	OnPlayerMoved.Broadcast(Rec.PlayerFrom, NewPos);
	return true;
}

// ==================================================================
// Queries
// ==================================================================

FPushBoxCellData UPushBoxGridManager::GetCellData(FIntPoint Coord) const
{
	const int32 Idx = CoordToIndex(Coord);
	return Cells.IsValidIndex(Idx) ? Cells[Idx] : FPushBoxCellData();
}

void UPushBoxGridManager::SetCellData(FIntPoint Coord, const FPushBoxCellData& NewData)
{
	const int32 Idx = CoordToIndex(Coord);
	if (Cells.IsValidIndex(Idx))
	{
		Cells[Idx] = NewData;
	}
}

bool UPushBoxGridManager::IsValidCoord(FIntPoint Coord) const
{
	return Coord.X >= 0 && Coord.X < GridSize.X
		&& Coord.Y >= 0 && Coord.Y < GridSize.Y;
}

bool UPushBoxGridManager::IsCellWalkable(FIntPoint Coord) const
{
	if (!IsValidCoord(Coord)) return false;
	const FPushBoxCellData& Cell = Cells[CoordToIndex(Coord)];
	// Player blocked only by Wall and boxes
	if (Cell.TileType == EPushBoxTileType::Wall) return false;
	if (HasBoxAt(Coord)) return false;
	return true;
}

bool UPushBoxGridManager::CanBoxEnterCell(FIntPoint Coord, EPushBoxDirection PushDir) const
{
	if (!IsValidCoord(Coord)) return false;
	const FPushBoxCellData& Cell = Cells[CoordToIndex(Coord)];

	if (Cell.TileType == EPushBoxTileType::Wall) return false;
	if (HasBoxAt(Coord)) return false;

	// OneWayDoor only lets a box in when pushed in the matching direction
	if (Cell.TileType == EPushBoxTileType::OneWayDoor &&
		Cell.OneWayDirection != PushDir)
	{
		return false;
	}
	return true;
}

bool UPushBoxGridManager::HasBoxAt(FIntPoint Coord) const
{
	return BoxPositions.Contains(Coord);
}

FVector UPushBoxGridManager::GridToWorld(FIntPoint Coord) const
{
	const FVector Origin = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	return Origin + FVector(Coord.X * CellSize.X, Coord.Y * CellSize.Y, 0.0);
}

FIntPoint UPushBoxGridManager::WorldToGrid(FVector WorldPos) const
{
	const FVector Origin = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	const FVector Local = WorldPos - Origin;
	return FIntPoint(
		FMath::FloorToInt32(Local.X / CellSize.X),
		FMath::FloorToInt32(Local.Y / CellSize.Y));
}

FPushBoxGridSnapshot UPushBoxGridManager::TakeSnapshot() const
{
	FPushBoxGridSnapshot Snap;
	Snap.GridSize       = GridSize;
	Snap.Cells          = Cells;
	Snap.PlayerPosition = PlayerGridPosition;
	Snap.BoxPositions   = BoxPositions;
	return Snap;
}

bool UPushBoxGridManager::CheckWinCondition() const
{
	if (BoxPositions.Num() == 0) return false;

	// 1) All boxes must be on target cells
	for (const FIntPoint& Pos : BoxPositions)
	{
		const int32 Idx = CoordToIndex(Pos);
		if (!Cells.IsValidIndex(Idx)) return false;
		if (!Cells[Idx].IsTarget())   return false;
	}

	// 2) Find PlayerGoal tile ！ if one exists, the player must be able to reach it
	FIntPoint GoalCoord(-1, -1);
	for (int32 i = 0; i < Cells.Num(); ++i)
	{
		if (Cells[i].TileType == EPushBoxTileType::PlayerGoal)
		{
			GoalCoord = FIntPoint(i % GridSize.X, i / GridSize.X);
			break;
		}
	}

	if (GoalCoord.X >= 0)
	{
		// BFS reachability from player to goal (blocked by Wall and boxes)
		TSet<int32> Visited;
		TArray<FIntPoint> Queue;
		Queue.Add(PlayerGridPosition);
		Visited.Add(CoordToIndex(PlayerGridPosition));

		static const FIntPoint Dirs[] = { {0,1}, {0,-1}, {1,0}, {-1,0} };

		while (Queue.Num() > 0)
		{
			FIntPoint Cur = Queue[0];
			Queue.RemoveAt(0);

			if (Cur == GoalCoord)
				return true;

			for (const FIntPoint& D : Dirs)
			{
				FIntPoint Next = Cur + D;
				if (!IsValidCoord(Next)) continue;
				int32 NIdx = CoordToIndex(Next);
				if (Visited.Contains(NIdx)) continue;
				if (Cells[NIdx].TileType == EPushBoxTileType::Wall) continue;
				if (HasBoxAt(Next)) continue;
				Visited.Add(NIdx);
				Queue.Add(Next);
			}
		}
		return false; // Player cannot reach the goal
	}

	// No PlayerGoal tile exists ！ classic sokoban rules (all boxes on targets = win)
	return true;
}

FIntPoint UPushBoxGridManager::GetDirectionOffset(EPushBoxDirection Direction)
{
	switch (Direction)
	{
	case EPushBoxDirection::North: return FIntPoint( 0,  1);
	case EPushBoxDirection::East:  return FIntPoint( 1,  0);
	case EPushBoxDirection::South: return FIntPoint( 0, -1);
	case EPushBoxDirection::West:  return FIntPoint(-1,  0);
	default:                       return FIntPoint( 0,  0);
	}
}

EPushBoxDirection UPushBoxGridManager::GetOppositeDirection(EPushBoxDirection Direction)
{
	switch (Direction)
	{
	case EPushBoxDirection::North: return EPushBoxDirection::South;
	case EPushBoxDirection::East:  return EPushBoxDirection::West;
	case EPushBoxDirection::South: return EPushBoxDirection::North;
	case EPushBoxDirection::West:  return EPushBoxDirection::East;
	default:                       return EPushBoxDirection::None;
	}
}

// ==================================================================
// Internal helpers
// ==================================================================

int32 UPushBoxGridManager::CoordToIndex(FIntPoint Coord) const
{
	if (!IsValidCoord(Coord)) return INDEX_NONE;
	return Coord.Y * GridSize.X + Coord.X;
}

void UPushBoxGridManager::SetPuzzleState(EPuzzleState NewState)
{
	if (CurrentPuzzleState == NewState) return;
	const EPuzzleState Old = CurrentPuzzleState;
	CurrentPuzzleState = NewState;
	OnPuzzleStateChanged.Broadcast(NewState);

	// Fire the completion delegate when transitioning to Completed
	if (NewState == EPuzzleState::Completed)
	{
		OnPuzzleCompleted.Broadcast();
	}

	if (UGameplayMessageSubsystem* Msg = GetMsgSubsystem(this))
	{
		FPushBoxMessage_PuzzleStateChanged M;
		M.OldState = Old;
		M.NewState = NewState;
		Msg->BroadcastMessage(PushBoxTags::Message_PuzzleStateChanged, M);
	}
}

void UPushBoxGridManager::BroadcastBoxMoved(FIntPoint From, FIntPoint To, EPushBoxDirection Dir)
{
	OnBoxMoved.Broadcast(From, To);

	if (UGameplayMessageSubsystem* Msg = GetMsgSubsystem(this))
	{
		FPushBoxMessage_BoxMoved M;
		M.FromCoord = From;
		M.ToCoord   = To;
		M.Direction = Dir;
		Msg->BroadcastMessage(PushBoxTags::Message_BoxMoved, M);
	}
}

void UPushBoxGridManager::BroadcastPlayerMoved(FIntPoint From, FIntPoint To, EPushBoxDirection Dir)
{
	OnPlayerMoved.Broadcast(From, To);

	if (UGameplayMessageSubsystem* Msg = GetMsgSubsystem(this))
	{
		FPushBoxMessage_PlayerMoved M;
		M.FromCoord = From;
		M.ToCoord   = To;
		M.Direction = Dir;
		Msg->BroadcastMessage(PushBoxTags::Message_PlayerMoved, M);
	}
}
