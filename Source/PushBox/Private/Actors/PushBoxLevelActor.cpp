// Copyright PushBox Game. All Rights Reserved.

#include "Actors/PushBoxLevelActor.h"
#include "Actors/PushBoxBox.h"
#include "Actors/PushBoxTileActorBase.h"
#include "Grid/PushBoxGridManager.h"
#include "Data/PuzzleLevelDataAsset.h"
#include "WFCGeneratorComponent.h"
#include "WFCAsset.h"
#include "WFCAssetModel.h"
#include "WFCTileAsset2D.h"
#include "Core/WFCGenerator.h"
#include "Core/WFCModel.h"
#include "Core/Grids/WFCGrid2D.h"
#include "Core/WFCTypes.h"
#include "Core/Constraints/WFCFixedTileConstraint.h"

APushBoxLevelActor::APushBoxLevelActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GridManager = CreateDefaultSubobject<UPushBoxGridManager>(TEXT("GridManager"));
}

bool APushBoxLevelActor::HasValidDataAsset() const
{
	if (!PuzzleLevelData) return false;
	const int32 ExpectedCells = PuzzleLevelData->GridSize.X * PuzzleLevelData->GridSize.Y;
	return PuzzleLevelData->StartState.Cells.Num() == ExpectedCells && ExpectedCells > 0;
}

void APushBoxLevelActor::BeginPlay()
{
	if (HasValidDataAsset())
	{
		// We have a valid DataAsset with saved cell data.
		// Disable auto-run ！ we will manually initialize, fix all cells, then run.
		if (WFCGenerator)
		{
			WFCGenerator->bAutoRun = false;
		}

		UE_LOG(LogPushBox, Log, TEXT("LevelActor: DataAsset has valid cell data (%dx%d, %d cells). Will fix grid cells from DataAsset."),
			PuzzleLevelData->GridSize.X, PuzzleLevelData->GridSize.Y,
			PuzzleLevelData->StartState.Cells.Num());

		Super::BeginPlay();

		// Initialize grid manager from DataAsset (cell types, boxes, player position)
		GridManager->InitializeFromDataAsset(PuzzleLevelData);

		// Fix all grid cells and run ！ it will collapse instantly and spawn actors.
		// OnGeneratorFinished (parent) spawns tile actors into SpawnedTileActors.
		RunWFCWithFixedCellsFromDataAsset();

		// At this point WFC has completed synchronously and the parent has spawned
		// all tile actors. Now configure them and spawn puzzle actors (boxes, targets).
		SpawnPuzzleActors();
		TeleportPlayerToGridPosition();
	}
	else
	{
		// No valid DataAsset ！ standard generation path.
		// Bind to native OnFinishedEvent so we run AFTER parent's OnGeneratorFinished.
		if (WFCGenerator)
		{
			WFCGenerator->OnFinishedEvent.AddUObject(this, &APushBoxLevelActor::OnWFCFinished);
		}

		Super::BeginPlay();
	}
}

void APushBoxLevelActor::BuildTileTypeLookup(TMap<EPushBoxTileType, int32>& OutLookup) const
{
	OutLookup.Empty();

	if (!WFCGenerator || !WFCGenerator->GetGenerator())
		return;

	const UWFCModel* Model = WFCGenerator->GetGenerator()->GetModel();
	if (!Model) return;

	const int32 NumTiles = Model->GetNumTiles();

	// Map: GameplayTag ★ EPushBoxTileType
	struct FTagToType
	{
		FGameplayTag Tag;
		EPushBoxTileType Type;
	};

	TArray<FTagToType> TagMappings;
	TagMappings.Add({ PushBoxTags::Tile_Wall, EPushBoxTileType::Wall });
	TagMappings.Add({ PushBoxTags::Tile_Ice, EPushBoxTileType::Ice });
	TagMappings.Add({ PushBoxTags::Tile_OneWayDoor, EPushBoxTileType::OneWayDoor });
	TagMappings.Add({ PushBoxTags::Tile_TargetMarker, EPushBoxTileType::TargetMarker });
	// Floor/Empty is the default (tiles with no special tag)

	for (int32 TileId = 0; TileId < NumTiles; ++TileId)
	{
		const FWFCModelAssetTile* AssetTile = Model->GetTile<FWFCModelAssetTile>(TileId);
		if (!AssetTile || !AssetTile->TileAsset.IsValid())
			continue;

		const UWFCTileAsset* TileAsset = AssetTile->TileAsset.Get();

		bool bMapped = false;
		for (const FTagToType& Mapping : TagMappings)
		{
			if (TileAsset->OwnedTags.HasTag(Mapping.Tag))
			{
				// Only store the first tile ID found for each type (rotation 0 preferred)
				if (!OutLookup.Contains(Mapping.Type) || AssetTile->Rotation == 0)
				{
					OutLookup.Add(Mapping.Type, TileId);
				}
				bMapped = true;
				break;
			}
		}

		// If no special tag matched, this is a floor tile
		if (!bMapped)
		{
			if (!OutLookup.Contains(EPushBoxTileType::Empty) || AssetTile->Rotation == 0)
			{
				OutLookup.Add(EPushBoxTileType::Empty, TileId);
			}
		}
	}

	UE_LOG(LogPushBox, Log, TEXT("BuildTileTypeLookup: Found %d tile type mappings"), OutLookup.Num());
	for (auto& Pair : OutLookup)
	{
		UE_LOG(LogPushBox, Log, TEXT("  Type %d -> TileId %d"), (int32)Pair.Key, Pair.Value);
	}
}

void APushBoxLevelActor::RunWFCWithFixedCellsFromDataAsset()
{
	if (!WFCGenerator || !PuzzleLevelData)
	{
		UE_LOG(LogPushBox, Error, TEXT("RunWFCWithFixedCellsFromDataAsset: Missing generator or PuzzleLevelData"));
		return;
	}

	// Step 1: Force re-init to create a brand-new generator with clean state.
	// This avoids stale FixedTileMappings / Finished state from editor duplication.
	if (!WFCGenerator->Initialize(true))
	{
		UE_LOG(LogPushBox, Error, TEXT("RunWFCWithFixedCellsFromDataAsset: Failed to initialize generator"));
		return;
	}

	UWFCGenerator* Generator = WFCGenerator->GetGenerator();
	if (!Generator)
	{
		UE_LOG(LogPushBox, Error, TEXT("RunWFCWithFixedCellsFromDataAsset: No generator after initialization"));
		return;
	}

	const UWFCGrid2D* Grid2D = Generator->GetGrid<UWFCGrid2D>();
	if (!Grid2D)
	{
		UE_LOG(LogPushBox, Error, TEXT("RunWFCWithFixedCellsFromDataAsset: Grid is not 2D"));
		return;
	}

	// Step 2: Build the lookup from EPushBoxTileType ★ TileId using OwnedTags
	TMap<EPushBoxTileType, int32> TileTypeLookup;
	BuildTileTypeLookup(TileTypeLookup);

	if (TileTypeLookup.Num() == 0)
	{
		UE_LOG(LogPushBox, Error, TEXT("RunWFCWithFixedCellsFromDataAsset: No tile type mappings found. "
			"Make sure tile assets have OwnedTags (Tile.Wall, Tile.Floor, etc.)"));
		return;
	}

	const int32* FloorTileIdPtr = TileTypeLookup.Find(EPushBoxTileType::Empty);
	if (!FloorTileIdPtr)
	{
		UE_LOG(LogPushBox, Warning, TEXT("RunWFCWithFixedCellsFromDataAsset: No floor tile found in model."));
	}

	// Step 3: Get the FixedTileConstraint to pin all cells
	UWFCFixedTileConstraint* FixedConstraint = Generator->GetConstraint<UWFCFixedTileConstraint>();

	// Step 4: For each cell in the DataAsset, assign the correct tile
	const FPushBoxGridSnapshot& StartState = PuzzleLevelData->StartState;
	const int32 SX = PuzzleLevelData->GridSize.X;
	const int32 SY = PuzzleLevelData->GridSize.Y;
	int32 FixedCount = 0;
	int32 UnmappedCount = 0;

	for (int32 Y = 0; Y < SY; ++Y)
	{
		for (int32 X = 0; X < SX; ++X)
		{
			const int32 DataIdx = Y * SX + X;
			if (!StartState.Cells.IsValidIndex(DataIdx))
				continue;

			const FPushBoxCellData& Cell = StartState.Cells[DataIdx];

			const FIntPoint GridLoc(X, Y);
			const int32 CellIndex = Grid2D->GetCellIndexForLocation(GridLoc);
			if (CellIndex == INDEX_NONE || !Generator->IsValidCellIndex(CellIndex))
				continue;

			// PlayerGoal uses floor tile (it's just a floor with special marking)
			EPushBoxTileType LookupType = Cell.TileType;
			if (LookupType == EPushBoxTileType::PlayerGoal && !TileTypeLookup.Contains(EPushBoxTileType::PlayerGoal))
			{
				LookupType = EPushBoxTileType::Empty;
			}

			const int32* TileIdPtr = TileTypeLookup.Find(LookupType);
			if (!TileIdPtr)
			{
				TileIdPtr = FloorTileIdPtr;
			}

			if (TileIdPtr)
			{
				if (FixedConstraint)
				{
					FixedConstraint->AddFixedTileMapping(CellIndex, *TileIdPtr);
				}
				else
				{
					Generator->Select(CellIndex, *TileIdPtr);
				}
				++FixedCount;
			}
			else
			{
				++UnmappedCount;
			}
		}
	}

	UE_LOG(LogPushBox, Log, TEXT("RunWFCWithFixedCellsFromDataAsset: Fixed %d/%d cells (%d unmapped)"),
		FixedCount, SX * SY, UnmappedCount);

	// Step 5: Run ！ all cells are pre-determined, spawns tile actors via normal pipeline
	Generator->Run(WFCGenerator->StepLimit);
}

void APushBoxLevelActor::OnWFCFinished(EWFCGeneratorState State)
{
	const bool bSuccess = (State == EWFCGeneratorState::Finished);
	if (!bSuccess)
	{
		UE_LOG(LogPushBox, Warning, TEXT("WFC generation finished with errors (state=%d)"), (int32)State);
	}

	if (PuzzleLevelData && HasValidDataAsset())
	{
		// Tile actors spawned from fixed cells.
		// GridManager was already initialized from DataAsset in BeginPlay.
		GridManager->SetPuzzleState(EPuzzleState::InProgress);

		UE_LOG(LogPushBox, Log, TEXT("LevelActor: Fixed-cell generation complete. Grid=%dx%d, Cells=%d, Boxes=%d"),
			GridManager->GridSize.X, GridManager->GridSize.Y,
			GridManager->Cells.Num(), GridManager->BoxPositions.Num());
	}
	else if (PuzzleLevelData)
	{
		// DataAsset exists but wasn't fully valid ！ ran randomly, overlay DataAsset data
		MapWFCResultsToCells();

		GridManager->PlayerGridPosition = PuzzleLevelData->StartState.PlayerPosition;
		GridManager->BoxPositions = PuzzleLevelData->StartState.BoxPositions;
		GridManager->MoveHistory.Empty();
		GridManager->MoveCount = 0;

		if (PuzzleLevelData->StartState.Cells.Num() == GridManager->Cells.Num())
		{
			for (int32 i = 0; i < GridManager->Cells.Num(); ++i)
			{
				// Overlay TargetMarker tile type from DataAsset onto randomly generated cells
				if (PuzzleLevelData->StartState.Cells[i].TileType == EPushBoxTileType::TargetMarker)
				{
					GridManager->Cells[i].TileType = EPushBoxTileType::TargetMarker;
				}
			}
		}

		GridManager->SetPuzzleState(EPuzzleState::InProgress);

		UE_LOG(LogPushBox, Log, TEXT("LevelActor: Random + DataAsset overlay. Grid=%dx%d, Cells=%d, Boxes=%d"),
			GridManager->GridSize.X, GridManager->GridSize.Y,
			GridManager->Cells.Num(), GridManager->BoxPositions.Num());
	}
	else
	{
		// No DataAsset ！ random generation mode
		MapWFCResultsToCells();
	}

	// SpawnPuzzleActors now handles ConfigureSpawnedTileActors + HandleBoxMoved binding internally
	SpawnPuzzleActors();

	// Teleport the player pawn to the correct grid position.
	// PlayerStart may not match the DataAsset's saved player position,
	// so we force-move the pawn to GridToWorld(PlayerGridPosition).
	TeleportPlayerToGridPosition();
}

void APushBoxLevelActor::SetupPuzzle()
{
	if (PuzzleLevelData)
	{
		GridManager->InitializeFromDataAsset(PuzzleLevelData);
	}
	else
	{
		MapWFCResultsToCells();
	}

	SpawnPuzzleActors();
}

void APushBoxLevelActor::GenerateAndSetup(int32 Seed)
{
	ClearPuzzleActors();
	DestroyAllSpawnedActors();
	RunNewGeneration(Seed);

	if (WFCGenerator && WFCGenerator->GetState() != EWFCGeneratorState::InProgress)
	{
		OnWFCGenerationComplete();
	}
}

void APushBoxLevelActor::RefreshFromDataAsset()
{
	if (!PuzzleLevelData || !HasValidDataAsset())
	{
		UE_LOG(LogPushBox, Warning, TEXT("RefreshFromDataAsset: No valid DataAsset"));
		return;
	}

	// 1. Clean up existing puzzle and tile actors
	ClearPuzzleActors();
	DestroyAllSpawnedActors();
	TileActorsByCoordIndex.Empty();

	// 2. Re-initialize grid manager from the (updated) DataAsset
	GridManager->InitializeFromDataAsset(PuzzleLevelData);

	// 2.5 Sync the WFC component's asset from the DataAsset reference,
	//     so Initialize(true) uses the correct grid dimensions.
	if (WFCGenerator && PuzzleLevelData->WFCAssetRef.IsValid())
	{
		UWFCAsset* LoadedAsset = PuzzleLevelData->WFCAssetRef.LoadSynchronous();
		if (LoadedAsset)
		{
			WFCGenerator->WFCAsset = LoadedAsset;
		}
	}

	// 3. Re-run WFC with fixed cells ！ this spawns all tile actors synchronously
	RunWFCWithFixedCellsFromDataAsset();

	// 4. Configure tile actors and spawn puzzle actors directly
	//    (WFC completes synchronously ！ parent's OnCellSelected already spawned tile actors)
	SpawnPuzzleActors();
	TeleportPlayerToGridPosition();

	UE_LOG(LogPushBox, Log, TEXT("RefreshFromDataAsset: Complete. Grid=%dx%d, Boxes=%d"),
		GridManager->GridSize.X, GridManager->GridSize.Y, GridManager->BoxPositions.Num());
}

void APushBoxLevelActor::OnWFCGenerationComplete()
{
	MapWFCResultsToCells();
	SpawnPuzzleActors();
}

void APushBoxLevelActor::MapWFCResultsToCells()
{
	if (!WFCGenerator) return;

	GridManager->SyncWithWFCGenerator();
	GridManager->InitializeBlankGrid(GridManager->GridSize);

	const UWFCGrid* Grid = WFCGenerator->GetGrid();
	if (!Grid) return;

	const UWFCGrid2D* Grid2D = Cast<UWFCGrid2D>(Grid);
	if (!Grid2D)
	{
		UE_LOG(LogPushBox, Error, TEXT("MapWFCResultsToCells: WFC grid is not a 2D grid"));
		return;
	}

	TArray<int32> TileIds;
	WFCGenerator->GetSelectedTileIds(TileIds);

	for (int32 CellIdx = 0; CellIdx < TileIds.Num(); ++CellIdx)
	{
		const FIntPoint GridCoord = Grid2D->GetLocationForCellIndex(CellIdx);
		if (!GridManager->IsValidCoord(GridCoord))
			continue;

		const FWFCModelAssetTile* AssetTile = GetAssetTileForCell(CellIdx);
		if (!AssetTile || !AssetTile->TileAsset.IsValid())
		{
			FPushBoxCellData CellData = GridManager->GetCellData(GridCoord);
			CellData.TileType = EPushBoxTileType::Empty;
			GridManager->SetCellData(GridCoord, CellData);
			continue;
		}

		const UWFCTileAsset* TileAsset = AssetTile->TileAsset.Get();
		FPushBoxCellData CellData = GridManager->GetCellData(GridCoord);

		if (TileAsset->OwnedTags.HasTag(PushBoxTags::Tile_Wall))
			CellData.TileType = EPushBoxTileType::Wall;
		else if (TileAsset->OwnedTags.HasTag(PushBoxTags::Tile_Ice))
			CellData.TileType = EPushBoxTileType::Ice;
		else if (TileAsset->OwnedTags.HasTag(PushBoxTags::Tile_OneWayDoor))
			CellData.TileType = EPushBoxTileType::OneWayDoor;
		else if (TileAsset->OwnedTags.HasTag(PushBoxTags::Tile_TargetMarker))
		{
			CellData.TileType = EPushBoxTileType::TargetMarker;
		}
		else
			CellData.TileType = EPushBoxTileType::Empty;

		GridManager->SetCellData(GridCoord, CellData);
	}

	GridManager->CurrentPuzzleState = EPuzzleState::InProgress;
}

void APushBoxLevelActor::SpawnPuzzleActors()
{
	ClearPuzzleActors();

	if (!GridManager) return;
	UWorld* World = GetWorld();
	if (!World) return;

	// Ensure tile actors are configured (direction rotation, grid data, lookup cache).
	// This is safe to call multiple times ！ it clears and rebuilds the cache.
	ConfigureSpawnedTileActors();

	// Ensure we're listening for box moves so tile actors get notified.
	// Use RemoveAll + AddUniqueDynamic to guarantee exactly one binding.
	GridManager->OnBoxMoved.RemoveAll(this);
	GridManager->OnBoxMoved.AddUniqueDynamic(this, &APushBoxLevelActor::HandleBoxMoved);

	// NOTE: Terrain tile actors (Floor, Wall, Ice, etc.) are spawned by the tile system
	// via AWFCTestingActor::SpawnActorForCell. We only spawn puzzle-specific actors here.

	// Spawn target markers
	for (int32 Idx = 0; Idx < GridManager->Cells.Num(); ++Idx)
	{
		const FPushBoxCellData& Cell = GridManager->Cells[Idx];
		if (Cell.IsTarget() && TargetMarkerClass)
		{
			int32 X = Idx % GridManager->GridSize.X;
			int32 Y = Idx / GridManager->GridSize.X;
			FVector WorldPos = GridManager->GridToWorld(FIntPoint(X, Y));

			FActorSpawnParameters Params;
			Params.Owner = this;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AActor* Marker = World->SpawnActor<AActor>(TargetMarkerClass, WorldPos, FRotator::ZeroRotator, Params);
			if (Marker)
			{
				SpawnedTargetActors.Add(Marker);
			}
		}
	}

	// Spawn box actors
	for (int32 i = 0; i < GridManager->BoxPositions.Num(); ++i)
	{
		if (!BoxActorClass) continue;

		const FIntPoint& BoxPos = GridManager->BoxPositions[i];
		FVector WorldPos = GridManager->GridToWorld(BoxPos) + FVector(0, 0, 50.f);

		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		APushBoxBox* BoxActor = World->SpawnActor<APushBoxBox>(BoxActorClass, WorldPos, FRotator::ZeroRotator, Params);
		if (BoxActor)
		{
			BoxActor->GridManager = GridManager;
			BoxActor->BoxIndex = i;
			SpawnedBoxActors.Add(BoxActor);
		}
	}

	// Notify tile actors about boxes that start on them.
	// This fires the initial OnBoxEntered event so that tiles like OneWayDoor
	// can set their visual state (e.g., flap pressed down) at level load.
	for (const FIntPoint& BoxPos : GridManager->BoxPositions)
	{
		if (APushBoxTileActorBase* Tile = GetPushBoxTileActorAtCoord(BoxPos))
		{
			Tile->NotifyBoxEntered(EPushBoxDirection::None);
		}
	}
}

void APushBoxLevelActor::ClearPuzzleActors()
{
	for (auto& Weak : SpawnedBoxActors)
	{
		if (APushBoxBox* Box = Weak.Get())
			Box->Destroy();
	}
	SpawnedBoxActors.Empty();

	for (auto& Weak : SpawnedTargetActors)
	{
		if (AActor* A = Weak.Get())
			A->Destroy();
	}
	SpawnedTargetActors.Empty();
}

// ==================================================================
// Tile Actor Configuration & Box Events
// ==================================================================

void APushBoxLevelActor::ConfigureSpawnedTileActors()
{
	TileActorsByCoordIndex.Empty();

	if (!GridManager || !WFCGenerator || !WFCGenerator->GetGenerator())
		return;

	const UWFCGrid2D* Grid2D = WFCGenerator->GetGenerator()->GetGrid<UWFCGrid2D>();
	if (!Grid2D) return;

	const int32 NumCells = WFCGenerator->GetGenerator()->GetNumCells();
	int32 ConfiguredCount = 0;
	int32 NonPBTileCount = 0;
	int32 TargetMarkerCount = 0;

	for (int32 CellIdx = 0; CellIdx < NumCells; ++CellIdx)
	{
		AActor* TileActor = GetActorForCell(CellIdx);
		if (!TileActor) continue;

		APushBoxTileActorBase* PBTile = Cast<APushBoxTileActorBase>(TileActor);
		if (!PBTile)
		{
			++NonPBTileCount;
			continue;
		}

		const FIntPoint GridCoord = Grid2D->GetLocationForCellIndex(CellIdx);
		if (!GridManager->IsValidCoord(GridCoord))
			continue;

		const FPushBoxCellData CellData = GridManager->GetCellData(GridCoord);

		if (CellData.TileType == EPushBoxTileType::TargetMarker)
		{
			++TargetMarkerCount;
		}

		// Initialize the tile with grid data ！ this applies direction rotation, etc.
		PBTile->InitializeTileData(GridManager, GridCoord, CellData);

		// Cache for fast lookup by coordinate
		const int32 CoordKey = GridCoord.Y * GridManager->GridSize.X + GridCoord.X;
		TileActorsByCoordIndex.Add(CoordKey, PBTile);

		++ConfiguredCount;
	}

	UE_LOG(LogPushBox, Log, TEXT("ConfigureSpawnedTileActors: Configured %d PushBoxTileActorBase instances (%d non-PBTile, %d TargetMarkers)"),
		ConfiguredCount, NonPBTileCount, TargetMarkerCount);
}

void APushBoxLevelActor::HandleBoxMoved(FIntPoint From, FIntPoint To)
{
	// Determine direction from From ★ To
	const FIntPoint Delta = To - From;
	EPushBoxDirection Dir = EPushBoxDirection::None;
	if (Delta.Y > 0) Dir = EPushBoxDirection::North;
	else if (Delta.Y < 0) Dir = EPushBoxDirection::South;
	else if (Delta.X > 0) Dir = EPushBoxDirection::East;
	else if (Delta.X < 0) Dir = EPushBoxDirection::West;

	// Notify the source tile that a box has left
	if (APushBoxTileActorBase* FromTile = GetPushBoxTileActorAtCoord(From))
	{
		FromTile->NotifyBoxExited(Dir);
	}

	// For ice slides (Manhattan distance > 1), notify all intermediate cells
	// The box passes through them: enter then immediately exit
	const FIntPoint Step = FIntPoint(FMath::Sign(Delta.X), FMath::Sign(Delta.Y));
	if (FMath::Abs(Delta.X) + FMath::Abs(Delta.Y) > 1)
	{
		FIntPoint Intermediate = From + Step;
		while (Intermediate != To)
		{
			if (APushBoxTileActorBase* MidTile = GetPushBoxTileActorAtCoord(Intermediate))
			{
				MidTile->NotifyBoxEntered(Dir);
				MidTile->NotifyBoxExited(Dir);
			}
			Intermediate = Intermediate + Step;
		}
	}

	// Notify the final destination tile that a box has entered
	if (APushBoxTileActorBase* ToTile = GetPushBoxTileActorAtCoord(To))
	{
		ToTile->NotifyBoxEntered(Dir);
	}
	else
	{
		UE_LOG(LogPushBox, Warning, TEXT("HandleBoxMoved: No PBTile at destination (%d,%d). TileActorsByCoordIndex has %d entries."),
			To.X, To.Y, TileActorsByCoordIndex.Num());
	}
}

APushBoxTileActorBase* APushBoxLevelActor::GetPushBoxTileActorAtCoord(FIntPoint Coord) const
{
	if (!GridManager || !GridManager->IsValidCoord(Coord))
		return nullptr;

	const int32 CoordKey = Coord.Y * GridManager->GridSize.X + Coord.X;
	const TWeakObjectPtr<APushBoxTileActorBase>* Found = TileActorsByCoordIndex.Find(CoordKey);
	if (Found && Found->IsValid())
	{
		return Found->Get();
	}
	return nullptr;
}

void APushBoxLevelActor::TeleportPlayerToGridPosition()
{
	if (!GridManager) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// Compute the correct world-space position for the player
	FVector TargetPos = GridManager->GridToWorld(GridManager->PlayerGridPosition);
	// Raise above ground for capsule half-height
	TargetPos.Z += 90.0f;

	// Try to find the player pawn and teleport it
	// Use a short timer because the pawn may not exist yet during BeginPlay
	FTimerHandle TimerHandle;
	TWeakObjectPtr<UPushBoxGridManager> WeakGM = GridManager;
	World->GetTimerManager().SetTimer(TimerHandle, [WeakGM, TargetPos, World]()
	{
		if (!World || !WeakGM.IsValid()) return;

		APlayerController* PC = World->GetFirstPlayerController();
		if (!PC) return;

		APawn* Pawn = PC->GetPawn();
		if (Pawn)
		{
			Pawn->SetActorLocation(TargetPos);
			UE_LOG(LogPushBox, Log, TEXT("TeleportPlayerToGridPosition: Moved pawn to (%.0f, %.0f, %.0f) for grid pos (%d,%d)"),
				TargetPos.X, TargetPos.Y, TargetPos.Z,
				WeakGM->PlayerGridPosition.X, WeakGM->PlayerGridPosition.Y);
		}
		else
		{
			UE_LOG(LogPushBox, Warning, TEXT("TeleportPlayerToGridPosition: No pawn found on player controller"));
		}
	}, 0.1f, false); // Short delay to ensure pawn is spawned
}
