// Copyright PushBox Game. All Rights Reserved.

#include "PuzzleEditorSubsystem.h"
#include "ObstacleGenerator.h"
#include "MCTSPuzzleGenerator.h"
#include "Actors/PushBoxLevelActor.h"
#include "Grid/PushBoxGridManager.h"
#include "Data/PuzzleLevelDataAsset.h"
#include "Utilities/PushBoxFunctionLibrary.h"
#include "WFCAsset.h"
#include "Core/Grids/WFCGrid2D.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/DataAssetFactory.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Editor.h"
#include "UObject/SavePackage.h"
#include "ObjectTools.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/MessageDialog.h"
#include "Misc/ConfigCacheIni.h"

static const TCHAR* GPuzzleEditorConfigSection = TEXT("PushBoxPuzzleEditor");
static const TCHAR* GPuzzleEditorConfigKey_LastAsset = TEXT("LastActivePuzzleAsset");
static const TCHAR* GPuzzleEditorConfigKey_Mode = TEXT("LastEditMode");

void UPuzzleEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ObstacleGen = NewObject<UObstacleGenerator>(this);
	TryRestoreLastSession();
}

void UPuzzleEditorSubsystem::Deinitialize()
{
	EditorGridManager = nullptr;
	ObstacleGen = nullptr;
	Super::Deinitialize();
}

UPuzzleLevelDataAsset* UPuzzleEditorSubsystem::CreateNewPuzzle(FString PuzzleName, FIntPoint GridSize)
{
	FString AssetName = PuzzleName.Replace(TEXT(" "), TEXT("_"));
	UPuzzleLevelDataAsset* PuzzleData = CreateOrFindDataAsset(AssetName);

	if (!PuzzleData)
	{
		UE_LOG(LogTemp, Error, TEXT("PuzzleEditor: Failed to create puzzle asset '%s'"), *AssetName);
		return nullptr;
	}

	// Reset all data
	PuzzleData->PuzzleName = FText::FromString(PuzzleName);
	PuzzleData->GridSize = GridSize;
	PuzzleData->StartState = FPushBoxGridSnapshot();
	PuzzleData->GoalState = FPushBoxGridSnapshot();
	PuzzleData->SolutionMoves.Empty();
	PuzzleData->ParMoveCount = 0;

	// Immediately save the empty asset to disk
	{
		FText Err;
		SaveDataAssetToDisk(PuzzleData, Err);
	}

	ActivePuzzleData = PuzzleData;

	EditorGridManager = NewObject<UPushBoxGridManager>(this);
	EditorGridManager->InitializeBlankGrid(GridSize);

	// Reset obstacle generator for the new puzzle
	if (ObstacleGen)
	{
		ObstacleGen->ClearSnapshots();
	}

	// Reset goal snapshot
	GoalSnapshot = FPushBoxGridSnapshot();

	CurrentMode = EPuzzleEditorMode::TilePlacement;

	// Sync with level actor
	SyncLevelActorDataAsset();

	return PuzzleData;
}

void UPuzzleEditorSubsystem::LoadPuzzleData(UPuzzleLevelDataAsset* InPuzzleData)
{
	if (!InPuzzleData)
		return;

	ActivePuzzleData = InPuzzleData;

	// Recreate the editor grid manager from the data asset
	EditorGridManager = NewObject<UPushBoxGridManager>(this);

	if (InPuzzleData->StartState.Cells.Num() == InPuzzleData->GridSize.X * InPuzzleData->GridSize.Y &&
		InPuzzleData->GridSize.X > 0)
	{
		EditorGridManager->InitializeBlankGrid(InPuzzleData->GridSize);
		for (int32 i = 0; i < InPuzzleData->StartState.Cells.Num(); ++i)
		{
			int32 X = i % InPuzzleData->GridSize.X;
			int32 Y = i / InPuzzleData->GridSize.X;
			EditorGridManager->SetCellData(FIntPoint(X, Y), InPuzzleData->StartState.Cells[i]);
		}
		EditorGridManager->PlayerGridPosition = InPuzzleData->StartState.PlayerPosition;
		EditorGridManager->BoxPositions = InPuzzleData->StartState.BoxPositions;
	}
	else
	{
		EditorGridManager->InitializeBlankGrid(InPuzzleData->GridSize);
	}

	// Restore goal snapshot
	if (InPuzzleData->GoalState.Cells.Num() == InPuzzleData->GridSize.X * InPuzzleData->GridSize.Y)
	{
		GoalSnapshot = InPuzzleData->GoalState;
	}
	else
	{
		GoalSnapshot = FPushBoxGridSnapshot();
	}

	// Reset obstacle generator
	if (ObstacleGen)
	{
		ObstacleGen->ClearSnapshots();
	}

	CurrentMode = EPuzzleEditorMode::TilePlacement;

	// Sync the WFC asset grid dimensions to match the new puzzle's grid size
	// (must happen before SyncLevelActorDataAsset which triggers RefreshFromDataAsset -> WFC run)
	SyncWFCGridSize(InPuzzleData->GridSize);

	// Sync with level actor
	SyncLevelActorDataAsset();

	SaveSessionToConfig();

	UE_LOG(LogTemp, Log, TEXT("PuzzleEditor: Loaded puzzle '%s' (%dx%d)"),
		*InPuzzleData->PuzzleName.ToString(), InPuzzleData->GridSize.X, InPuzzleData->GridSize.Y);
}

bool UPuzzleEditorSubsystem::GenerateTerrain(int32 Seed)
{
	UPushBoxGridManager* GM = GetActiveGridManager();
	if (!GM) return false;

	FRandomStream Rng(Seed);
	const int32 SX = GM->GridSize.X;
	const int32 SY = GM->GridSize.Y;

	GM->BoxPositions.Empty();

	for (int32 Y = 0; Y < SY; ++Y)
	{
		for (int32 X = 0; X < SX; ++X)
		{
			FIntPoint Coord(X, Y);
			const bool bBorder = (X == 0 || Y == 0 || X == SX - 1 || Y == SY - 1);

			FPushBoxCellData Cell;
			if (bBorder)
			{
				Cell.TileType = EPushBoxTileType::Wall;
			}
			else
			{
				float Roll = Rng.FRand();
				if (Roll < 0.12f)
					Cell.TileType = EPushBoxTileType::Wall;
				else if (Roll < 0.80f)
					Cell.TileType = EPushBoxTileType::Empty;
				else if (Roll < 0.88f)
					Cell.TileType = EPushBoxTileType::Ice;
				else if (Roll < 0.95f)
				{
					Cell.TileType = EPushBoxTileType::OneWayDoor;
					Cell.OneWayDirection = static_cast<EPushBoxDirection>(Rng.RandRange(0, 3));
				}
				else
				{
					Cell.TileType = EPushBoxTileType::TargetMarker;
					Cell.bHasBox = true;
				}
			}
			GM->SetCellData(Coord, Cell);
		}
	}

	for (int32 Y = 1; Y < SY - 1; ++Y)
	{
		for (int32 X = 1; X < SX - 1; ++X)
		{
			FPushBoxCellData D = GM->GetCellData(FIntPoint(X, Y));
			if (D.TileType == EPushBoxTileType::TargetMarker)
			{
				GM->BoxPositions.AddUnique(FIntPoint(X, Y));
			}
		}
	}

	TArray<FIntPoint> FloorCoords;
	for (int32 Y = 1; Y < SY - 1; ++Y)
		for (int32 X = 1; X < SX - 1; ++X)
		{
			FPushBoxCellData D = GM->GetCellData(FIntPoint(X, Y));
			if (D.TileType == EPushBoxTileType::Empty)
				FloorCoords.Add(FIntPoint(X, Y));
		}

	if (FloorCoords.Num() == 0)
	{
		FPushBoxCellData C;
		C.TileType = EPushBoxTileType::Empty;
		GM->SetCellData(FIntPoint(1, 1), C);
		FloorCoords.Add(FIntPoint(1, 1));
	}

	const int32 PlayerIdx = Rng.RandRange(0, FloorCoords.Num() - 1);
	GM->PlayerGridPosition = FloorCoords[PlayerIdx];

	return true;
}

void UPuzzleEditorSubsystem::SetEditMode(EPuzzleEditorMode NewMode)
{
	UPushBoxGridManager* GM = GetActiveGridManager();

	if (CurrentMode == EPuzzleEditorMode::GoalState && NewMode == EPuzzleEditorMode::ReverseEdit)
	{
		if (GM)
		{
			GoalSnapshot = GM->TakeSnapshot();
			if (ActivePuzzleData)
			{
				ActivePuzzleData->GoalState = GoalSnapshot;
			}
			GM->CurrentPuzzleState = EPuzzleState::InProgress;
		}
	}

	if (NewMode == EPuzzleEditorMode::PlayTest && GM)
	{
		GM->CurrentPuzzleState = EPuzzleState::InProgress;
	}

	CurrentMode = NewMode;
}

bool UPuzzleEditorSubsystem::RemoveBox(FIntPoint Coord)
{
	UPushBoxGridManager* GM = GetActiveGridManager();
	if (!GM) return false;

	int32 Idx = GM->BoxPositions.IndexOfByKey(Coord);
	if (Idx == INDEX_NONE) return false;

	GM->BoxPositions.RemoveAt(Idx);

	FPushBoxCellData Cell = GM->GetCellData(Coord);
	Cell.bHasBox = false;
	Cell.TileType = EPushBoxTileType::Empty;
	GM->SetCellData(Coord, Cell);

	return true;
}

void UPuzzleEditorSubsystem::SetPlayerStart(FIntPoint Coord)
{
	UPushBoxGridManager* GM = GetActiveGridManager();
	if (!GM) return;

	if (!GM->IsValidCoord(Coord)) return;
	FPushBoxCellData Cell = GM->GetCellData(Coord);
	if (Cell.TileType == EPushBoxTileType::Wall) return;

	GM->PlayerGridPosition = Coord;

	SyncPlayerStartToGrid();
}

void UPuzzleEditorSubsystem::SetCellType(FIntPoint Coord, EPushBoxTileType Type)
{
	UPushBoxGridManager* GM = GetActiveGridManager();
	if (!GM) return;

	FPushBoxCellData OldCell = GM->GetCellData(Coord);

	if (OldCell.TileType == EPushBoxTileType::TargetMarker && Type != EPushBoxTileType::TargetMarker)
	{
		GM->BoxPositions.Remove(Coord);
	}

	FPushBoxCellData Cell = OldCell;
	Cell.TileType = Type;

	if (Type == EPushBoxTileType::TargetMarker)
	{
		Cell.bHasBox = true;
		GM->BoxPositions.AddUnique(Coord);
	}
	else
	{
		if (OldCell.TileType == EPushBoxTileType::TargetMarker)
		{
			Cell.bHasBox = false;
		}
	}

	GM->SetCellData(Coord, Cell);
}

void UPuzzleEditorSubsystem::SetCellOneWayDirection(FIntPoint Coord, EPushBoxDirection Direction)
{
	UPushBoxGridManager* GM = GetActiveGridManager();
	if (!GM) return;

	FPushBoxCellData Cell = GM->GetCellData(Coord);
	Cell.OneWayDirection = Direction;
	GM->SetCellData(Coord, Cell);
}

bool UPuzzleEditorSubsystem::ReversePullBox(FIntPoint BoxCoord, EPushBoxDirection Direction)
{
	UPushBoxGridManager* GM = GetActiveGridManager();
	if (!GM || CurrentMode != EPuzzleEditorMode::ReverseEdit)
		return false;

	const FIntPoint OldPlayerPos = GM->PlayerGridPosition;
	const FIntPoint RequiredPlayerPos = BoxCoord + UPushBoxGridManager::GetDirectionOffset(Direction);

	if (OldPlayerPos != RequiredPlayerPos)
	{
		if (!CanPlayerWalkTo(GM, OldPlayerPos, RequiredPlayerPos))
		{
			return false;
		}
	}

	bool bSuccess = GM->TryReversePullBox(BoxCoord, Direction);

	if (bSuccess && ObstacleGen && GM->MoveHistory.Num() > 0)
	{
		ObstacleGen->RecordSnapshot(GM, GM->MoveHistory.Last(), OldPlayerPos);
	}

	return bSuccess;
}

bool UPuzzleEditorSubsystem::ReversePlayerMove(EPushBoxDirection Direction)
{
	UPushBoxGridManager* GM = GetActiveGridManager();
	if (!GM || CurrentMode != EPuzzleEditorMode::ReverseEdit)
		return false;

	return GM->TryReversePlayerMove(Direction);
}

bool UPuzzleEditorSubsystem::UndoReversePull()
{
	UPushBoxGridManager* GM = GetActiveGridManager();
	if (!GM) return false;

	bool bSuccess = GM->UndoLastMove();

	if (bSuccess && ObstacleGen)
	{
		ObstacleGen->UndoLastSnapshot();
	}

	return bSuccess;
}

bool UPuzzleEditorSubsystem::ValidateAndSave(FText& OutError)
{
	UPushBoxGridManager* GM = GetActiveGridManager();
	if (!GM)
	{
		OutError = FText::FromString(TEXT("No active grid manager."));
		FMessageDialog::Open(EAppMsgType::Ok, OutError);
		return false;
	}

	if (!ActivePuzzleData)
	{
		OutError = FText::FromString(TEXT("No active puzzle data asset. Please select or create one first."));
		FMessageDialog::Open(EAppMsgType::Ok, OutError);
		return false;
	}

	// Current state IS the start state (after reverse-pulls)
	ActivePuzzleData->StartState = GM->TakeSnapshot();
	ActivePuzzleData->GridSize = GM->GridSize;

	// Clean up per-cell flags that are tracked separately:
	// bHasBox is tracked via BoxPositions array, not per-cell
	for (FPushBoxCellData& Cell : ActivePuzzleData->StartState.Cells)
	{
		Cell.bHasBox = false;
	}

	// GoalState: use the snapshot taken when switching Goal★Reverse.
	if (GoalSnapshot.Cells.Num() != GM->GridSize.X * GM->GridSize.Y)
	{
		OutError = FText::FromString(
			TEXT("No goal state recorded! Please follow the workflow:\n"
				 "1. Place boxes on targets in 'Goal' mode\n"
				 "2. Switch to 'Reverse' mode (this captures the goal state)\n"
				 "3. Pull boxes to define start positions\n"
				 "4. Save"));
		FMessageDialog::Open(EAppMsgType::Ok, OutError);
		return false;
	}

	// Build GoalState: use CURRENT terrain (with obstacles) but
	// restore the goal's box/player positions and target cell types.
	{
		FPushBoxGridSnapshot GoalToSave;
		GoalToSave.GridSize = GM->GridSize;
		GoalToSave.Cells = GM->Cells;
		GoalToSave.PlayerPosition = GoalSnapshot.PlayerPosition;
		GoalToSave.BoxPositions = GoalSnapshot.BoxPositions;

		// Overlay TargetMarker tile types from goal snapshot
		for (int32 i = 0; i < GoalSnapshot.Cells.Num() && i < GoalToSave.Cells.Num(); ++i)
		{
			if (GoalSnapshot.Cells[i].IsTarget())
			{
				GoalToSave.Cells[i].TileType = EPushBoxTileType::TargetMarker;
			}
		}

		ActivePuzzleData->GoalState = GoalToSave;
	}

	// Clean up GoalState per-cell flags
	for (FPushBoxCellData& Cell : ActivePuzzleData->GoalState.Cells)
	{
		Cell.bHasBox = false;
	}

	// Count target cells ！ must equal the number of boxes
	int32 TargetCount = 0;
	for (const FPushBoxCellData& Cell : GoalSnapshot.Cells)
	{
		if (Cell.IsTarget()) ++TargetCount;
	}

	if (TargetCount != GoalSnapshot.BoxPositions.Num())
	{
		OutError = FText::FromString(FString::Printf(
			TEXT("Target count (%d) does not match box count (%d)!\n"
				 "In Goal mode, each box must be placed on exactly one target."),
			TargetCount, GoalSnapshot.BoxPositions.Num()));
		FMessageDialog::Open(EAppMsgType::Ok, OutError);
		return false;
	}

	// Convert reverse moves to forward solution
	ActivePuzzleData->SolutionMoves = UPushBoxFunctionLibrary::ReverseMoveSequence(GM->MoveHistory);
	ActivePuzzleData->ParMoveCount = ActivePuzzleData->SolutionMoves.Num();

	// Validate ！ show message dialog on failure with option to force save
	if (!ActivePuzzleData->ValidatePuzzle(OutError))
	{
		EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo,
			FText::Format(NSLOCTEXT("PuzzleEditor", "SaveBlocked",
				"Validation Failed!\n\n{0}\n\nDo you want to force save anyway?"), OutError));

		if (Result != EAppReturnType::Yes)
		{
			return false;
		}
	}

	// Sync player start position in the level
	SyncPlayerStartToGrid();

	// Sync with level actor
	SyncLevelActorDataAsset();

	// Save to disk
	return SaveDataAssetToDisk(ActivePuzzleData, OutError);
}

UPuzzleLevelDataAsset* UPuzzleEditorSubsystem::CreateOrFindDataAsset(const FString& AssetName)
{
	FString PackagePath = TEXT("/Game/PushBox/Puzzles");
	FString FullPackageName = PackagePath / AssetName;
	FString FullAssetPath = FullPackageName + TEXT(".") + AssetName;

	// Try to load existing
	if (UObject* Existing = StaticLoadObject(UPuzzleLevelDataAsset::StaticClass(), nullptr, *FullAssetPath))
	{
		if (UPuzzleLevelDataAsset* Found = Cast<UPuzzleLevelDataAsset>(Existing))
		{
			return Found;
		}
	}

	// Create new
	UPackage* Package = CreatePackage(*FullPackageName);
	if (!Package)
	{
		UE_LOG(LogTemp, Error, TEXT("PuzzleEditor: Failed to create package '%s'"), *FullPackageName);
		return nullptr;
	}

	Package->FullyLoad();
	UPuzzleLevelDataAsset* NewAsset = NewObject<UPuzzleLevelDataAsset>(
		Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);

	if (NewAsset)
	{
		FAssetRegistryModule::AssetCreated(NewAsset);
		UE_LOG(LogTemp, Log, TEXT("PuzzleEditor: Created DataAsset '%s'"), *NewAsset->GetPathName());
	}

	return NewAsset;
}

bool UPuzzleEditorSubsystem::SaveDataAssetToDisk(UPuzzleLevelDataAsset* Asset, FText& OutError)
{
	if (!Asset)
	{
		OutError = FText::FromString(TEXT("No asset to save."));
		return false;
	}

	Asset->MarkPackageDirty();

	UPackage* Package = Asset->GetOutermost();
	FString PackageFilename;
	if (!FPackageName::TryConvertLongPackageNameToFilename(
			Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
	{
		OutError = FText::FromString(FString::Printf(
			TEXT("Failed to convert package name '%s' to filename."), *Package->GetName()));
		FMessageDialog::Open(EAppMsgType::Ok, OutError);
		return false;
	}

	// Ensure directory exists on disk
	FString Dir = FPaths::GetPath(PackageFilename);
	IFileManager::Get().MakeDirectory(*Dir, true);

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	bool bSaved = UPackage::SavePackage(Package, Asset, *PackageFilename, SaveArgs);

	UE_LOG(LogTemp, Log, TEXT("PuzzleEditor: SavePackage '%s' -> '%s' (Success=%d)"),
		*Package->GetName(), *PackageFilename, bSaved ? 1 : 0);

	if (!bSaved)
	{
		OutError = FText::FromString(FString::Printf(
			TEXT("SavePackage failed for '%s'"), *PackageFilename));
		FMessageDialog::Open(EAppMsgType::Ok, OutError);
		return false;
	}

	// Persist session config
	SaveSessionToConfig();

	OutError = FText::GetEmpty();
	return true;
}

void UPuzzleEditorSubsystem::SetActiveLevelActor(APushBoxLevelActor* InActor)
{
	ActiveLevelActor = InActor;
	if (InActor)
	{
		ActivePuzzleData = InActor->PuzzleLevelData;
		// Use the level actor's grid manager for editing
		EditorGridManager = nullptr;
	}
}

UPushBoxGridManager* UPuzzleEditorSubsystem::GetActiveGridManager() const
{
	// Prefer standalone editor grid, fall back to level actor's grid
	if (EditorGridManager)
		return EditorGridManager;
	return ActiveLevelActor ? ActiveLevelActor->GridManager : nullptr;
}

APushBoxLevelActor* UPuzzleEditorSubsystem::FindAndBindLevelActor()
{
	if (ActiveLevelActor && IsValid(ActiveLevelActor))
		return ActiveLevelActor;

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
		return nullptr;

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(World, APushBoxLevelActor::StaticClass(), Found);

	if (Found.Num() > 0)
	{
		ActiveLevelActor = Cast<APushBoxLevelActor>(Found[0]);
		UE_LOG(LogTemp, Log, TEXT("PuzzleEditor: Auto-bound to level actor '%s'"),
			*ActiveLevelActor->GetName());
	}

	return ActiveLevelActor;
}

void UPuzzleEditorSubsystem::SyncLevelActorDataAsset()
{
	APushBoxLevelActor* LA = FindAndBindLevelActor();
	if (!LA)
		return;

	if (ActivePuzzleData)
	{
		LA->PuzzleLevelData = ActivePuzzleData;
		LA->MarkPackageDirty();

		// Trigger a full refresh: re-init grid, rebuild WFC tile actors,
		// apply direction rotations, spawn box/target actors, etc.
		// This ensures manual puzzles get the same treatment as MCTS-generated ones.
		LA->RefreshFromDataAsset();

		UE_LOG(LogTemp, Log, TEXT("PuzzleEditor: Synced and refreshed level actor '%s' -> DataAsset '%s'"),
			*LA->GetName(), *ActivePuzzleData->GetName());
	}
}

// ==================================================================
// Session Persistence
// ==================================================================

void UPuzzleEditorSubsystem::TryRestoreLastSession()
{
	FString LastAssetPath;
	if (!GConfig->GetString(GPuzzleEditorConfigSection, GPuzzleEditorConfigKey_LastAsset, LastAssetPath, GEditorPerProjectIni))
		return;

	if (LastAssetPath.IsEmpty())
		return;

	UPuzzleLevelDataAsset* PuzzleData = Cast<UPuzzleLevelDataAsset>
		(StaticLoadObject(UPuzzleLevelDataAsset::StaticClass(), nullptr, *LastAssetPath));
	if (!PuzzleData)
	{
		UE_LOG(LogTemp, Log, TEXT("PuzzleEditor: Could not restore last session asset: %s"), *LastAssetPath);
		return;
	}

	ActivePuzzleData = PuzzleData;

	// Recreate the editor grid manager from the data asset's current start state
	EditorGridManager = NewObject<UPushBoxGridManager>(this);

	if (PuzzleData->StartState.Cells.Num() == PuzzleData->GridSize.X * PuzzleData->GridSize.Y &&
		PuzzleData->GridSize.X > 0)
	{
		// Restore from StartState
		EditorGridManager->InitializeBlankGrid(PuzzleData->GridSize);
		for (int32 i = 0; i < PuzzleData->StartState.Cells.Num(); ++i)
		{
			int32 X = i % PuzzleData->GridSize.X;
			int32 Y = i / PuzzleData->GridSize.X;
			EditorGridManager->SetCellData(FIntPoint(X, Y), PuzzleData->StartState.Cells[i]);
		}
		EditorGridManager->PlayerGridPosition = PuzzleData->StartState.PlayerPosition;
		EditorGridManager->BoxPositions = PuzzleData->StartState.BoxPositions;
	}
	else
	{
		EditorGridManager->InitializeBlankGrid(PuzzleData->GridSize);
	}

	// Restore goal snapshot
	if (PuzzleData->GoalState.Cells.Num() == PuzzleData->GridSize.X * PuzzleData->GridSize.Y)
	{
		GoalSnapshot = PuzzleData->GoalState;
	}

	// Restore mode
	FString ModeStr;
	if (GConfig->GetString(GPuzzleEditorConfigSection, GPuzzleEditorConfigKey_Mode, ModeStr, GEditorPerProjectIni))
	{
		int32 ModeInt = FCString::Atoi(*ModeStr);
		CurrentMode = static_cast<EPuzzleEditorMode>(FMath::Clamp(ModeInt, 0, 3));
	}
	else
	{
		CurrentMode = EPuzzleEditorMode::TilePlacement;
	}

	UE_LOG(LogTemp, Log, TEXT("PuzzleEditor: Restored session from '%s' (%dx%d, mode=%d)"),
		*LastAssetPath, PuzzleData->GridSize.X, PuzzleData->GridSize.Y, (int32)CurrentMode);
}

void UPuzzleEditorSubsystem::SaveSessionToConfig() const
{
	if (!ActivePuzzleData)
		return;

	const FString AssetPath = ActivePuzzleData->GetPathName();
	GConfig->SetString(GPuzzleEditorConfigSection, GPuzzleEditorConfigKey_LastAsset, *AssetPath, GEditorPerProjectIni);
	GConfig->SetString(GPuzzleEditorConfigSection, GPuzzleEditorConfigKey_Mode,
		*FString::FromInt((int32)CurrentMode), GEditorPerProjectIni);
	GConfig->Flush(false, GEditorPerProjectIni);
}

// ==================================================================
// PlayerStart Sync
// ==================================================================

APlayerStart* UPuzzleEditorSubsystem::FindOrSpawnPlayerStart() const
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return nullptr;

	// Find existing PlayerStart
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(World, APlayerStart::StaticClass(), Found);
	if (Found.Num() > 0)
	{
		return Cast<APlayerStart>(Found[0]);
	}

	// Spawn one if none exists
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
}

void UPuzzleEditorSubsystem::SyncPlayerStartToGrid()
{
	UPushBoxGridManager* GM = GetActiveGridManager();
	if (!GM) return;

	APlayerStart* PS = FindOrSpawnPlayerStart();
	if (!PS) return;

	// For the editor standalone grid manager, GridToWorld returns positions
	// relative to FVector::ZeroVector (no owner actor). We need to find the
	// actual WFC level actor in the world to get the correct transform.
	FVector WorldPos = FVector::ZeroVector;

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (World)
	{
		// Find the PushBoxLevelActor to get its transform and cell size
		TArray<AActor*> LevelActors;
		UGameplayStatics::GetAllActorsOfClass(World, APushBoxLevelActor::StaticClass(), LevelActors);

		if (LevelActors.Num() > 0 && LevelActors[0])
		{
			APushBoxLevelActor* LevelActor = Cast<APushBoxLevelActor>(LevelActors[0]);
			if (LevelActor && LevelActor->GridManager)
			{
				// Use the level actor's grid manager for proper world coordinates
				WorldPos = LevelActor->GridManager->GridToWorld(GM->PlayerGridPosition);
			}
			else
			{
				// Fallback: use actor location + grid offset with CellSize
				FVector ActorLoc = LevelActors[0]->GetActorLocation();
				WorldPos = ActorLoc + FVector(
					GM->PlayerGridPosition.X * GM->CellSize.X,
					GM->PlayerGridPosition.Y * GM->CellSize.Y,
					0.0);
			}
		}
		else
		{
			// No level actor found, use raw grid coordinates
			WorldPos = FVector(
				GM->PlayerGridPosition.X * GM->CellSize.X,
				GM->PlayerGridPosition.Y * GM->CellSize.Y,
				0.0);
		}
	}

	// Raise slightly above ground for the capsule
	WorldPos.Z += 90.0f;
	PS->SetActorLocation(WorldPos);

	UE_LOG(LogTemp, Log, TEXT("PuzzleEditor: Synced PlayerStart to grid (%d,%d) -> world (%.0f, %.0f, %.0f)"),
		GM->PlayerGridPosition.X, GM->PlayerGridPosition.Y, WorldPos.X, WorldPos.Y, WorldPos.Z);
}

bool UPuzzleEditorSubsystem::CanPlayerWalkTo(const UPushBoxGridManager* GM, FIntPoint Start, FIntPoint Goal)
{
	if (!GM) return false;
	if (Start == Goal) return true;
	if (!GM->IsValidCoord(Start) || !GM->IsValidCoord(Goal)) return false;

	// BFS: player blocked only by walls during reverse mode
	const int32 SX = GM->GridSize.X;
	TSet<int32> Visited;
	TArray<FIntPoint> Queue;
	Queue.Add(Start);
	Visited.Add(Start.Y * SX + Start.X);

	static const FIntPoint Dirs[] = { {0,1}, {0,-1}, {1,0}, {-1,0} };

	while (Queue.Num() > 0)
	{
		FIntPoint Cur = Queue[0];
		Queue.RemoveAt(0, EAllowShrinking::No);

		for (const FIntPoint& D : Dirs)
		{
			FIntPoint Next = Cur + D;
			if (!GM->IsValidCoord(Next)) continue;
			int32 NIdx = Next.Y * SX + Next.X;
			if (Visited.Contains(NIdx)) continue;

			const FPushBoxCellData Cell = GM->GetCellData(Next);
			if (Cell.TileType == EPushBoxTileType::Wall) continue;

			if (Next == Goal) return true;

			Visited.Add(NIdx);
			Queue.Add(Next);
		}
	}
	return false;
}

// ==================================================================
// Obstacle Generation (solvable-guaranteed)
// ==================================================================

int32 UPuzzleEditorSubsystem::GenerateObstacles(const FObstacleGeneratorConfig& Config)
{
	UPushBoxGridManager* GM = GetActiveGridManager();
	if (!GM || !ObstacleGen)
		return 0;

	if (ObstacleGen->GetSnapshotCount() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("PuzzleEditor: No reverse snapshots recorded. "
			"Pull boxes in Reverse mode first, then generate obstacles."));
		return 0;
	}

	if (GoalSnapshot.Cells.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("PuzzleEditor: No goal snapshot. "
			"Place boxes in Goal mode and switch to Reverse mode first."));
		return 0;
	}

	// Analyze which cells are safe for obstacle placement
	ObstacleGen->AnalyzeSafetyZones(GM->GridSize);

	// Generate obstacles on safe cells
	int32 Count = ObstacleGen->GenerateObstacles(GM, GoalSnapshot, Config);

	UE_LOG(LogTemp, Log, TEXT("PuzzleEditor: Generated %d obstacles (%d snapshots analyzed)"),
		Count, ObstacleGen->GetSnapshotCount());

	return Count;
}

bool UPuzzleEditorSubsystem::CheckSolvability() const
{
	const UPushBoxGridManager* GM = GetActiveGridManager();
	if (!GM || !ObstacleGen)
		return false;

	if (ObstacleGen->GetSnapshotCount() == 0)
		return false;

	return ObstacleGen->ValidateForwardSolvability(GM, GoalSnapshot);
}

// ==================================================================
// MCTS Auto-Generation
// ==================================================================

FMCTSResult UPuzzleEditorSubsystem::MCTSAutoGenerate(const FMCTSConfig& Config)
{
	UMCTSPuzzleGenerator* Generator = NewObject<UMCTSPuzzleGenerator>(this);

	// Adaptive retry: progressively relax constraints if generation fails.
	// Each round loosens one parameter; we try up to 4 rounds total.
	FMCTSConfig AdaptiveConfig = Config;
	FMCTSResult Result;
	int32 TotalSimsAcrossRetries = 0;
	float TotalTimeAcrossRetries = 0.f;

	for (int32 Round = 0; Round < 4; ++Round)
	{
		if (Round > 0)
		{
			// Progressively relax constraints each round
			switch (Round)
			{
			case 1:
				// Round 1: Lower displacement, increase time
				AdaptiveConfig.MinBoxDisplacement = FMath::Max(2, AdaptiveConfig.MinBoxDisplacement - 1);
				AdaptiveConfig.TimeBudgetSeconds = Config.TimeBudgetSeconds * 1.5f;
				UE_LOG(LogTemp, Log, TEXT("MCTS Retry round %d: MinDisplacement=%d, Time=%.1fs"),
					Round, AdaptiveConfig.MinBoxDisplacement, AdaptiveConfig.TimeBudgetSeconds);
				break;

			case 2:
				// Round 2: Lower min pulls, open room more
				AdaptiveConfig.MinReversePulls = FMath::Max(2, AdaptiveConfig.MinReversePulls - 2);
				AdaptiveConfig.FloorRatio = FMath::Min(0.95f, AdaptiveConfig.FloorRatio + 0.05f);
				UE_LOG(LogTemp, Log, TEXT("MCTS Retry round %d: MinPulls=%d, FloorRatio=%.2f"),
					Round, AdaptiveConfig.MinReversePulls, AdaptiveConfig.FloorRatio);
				break;

			case 3:
				// Round 3: Last resort ！ minimal constraints
				AdaptiveConfig.MinBoxDisplacement = 2;
				AdaptiveConfig.MinReversePulls = 2;
				AdaptiveConfig.TimeBudgetSeconds = Config.TimeBudgetSeconds * 2.0f;
				AdaptiveConfig.FloorRatio = FMath::Min(0.95f, AdaptiveConfig.FloorRatio + 0.05f);
				UE_LOG(LogTemp, Log, TEXT("MCTS Retry round %d: LAST RESORT ！ MinDisp=2, MinPulls=2, Time=%.1fs, FloorRatio=%.2f"),
					Round, AdaptiveConfig.TimeBudgetSeconds, AdaptiveConfig.FloorRatio);
				break;
			}

			// Use a new random seed for each retry so we don't repeat the same failures
			if (AdaptiveConfig.Seed != 0)
				AdaptiveConfig.Seed += Round * 31;
		}

		Result = Generator->Generate(AdaptiveConfig);
		TotalSimsAcrossRetries += Result.SimulationCount;
		TotalTimeAcrossRetries += Result.ElapsedSeconds;

		if (Result.bSuccess)
		{
			if (Round > 0)
			{
				UE_LOG(LogTemp, Log, TEXT("MCTS: Succeeded on retry round %d (relaxed constraints)."), Round);
			}
			break;
		}
	}

	// Patch total stats into result so the UI shows cumulative numbers
	Result.SimulationCount = TotalSimsAcrossRetries;
	Result.ElapsedSeconds = TotalTimeAcrossRetries;

	if (!Result.bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("PuzzleEditor: MCTS auto-generation failed after all retry rounds."));
		return Result;
	}

	// Apply the result to the editor grid manager
	UPushBoxGridManager* GM = GetActiveGridManager();
	if (!GM)
	{
		// Create a new grid manager if needed
		EditorGridManager = NewObject<UPushBoxGridManager>(this);
		GM = EditorGridManager;
	}

	// Set up the grid from the start state
	GM->GridSize = Result.StartState.GridSize;
	GM->Cells = Result.StartState.Cells;
	GM->BoxPositions = Result.StartState.BoxPositions;
	GM->PlayerGridPosition = Result.StartState.PlayerPosition;
	GM->MoveHistory = Result.ReverseMoves;
	GM->MoveCount = Result.ReverseMoves.Num();
	GM->CurrentPuzzleState = EPuzzleState::InProgress;

	// Store the goal snapshot so Save works correctly
	GoalSnapshot = Result.GoalState;

	// Reset obstacle generator and re-record the snapshots for validation
	if (ObstacleGen)
	{
		ObstacleGen->ClearSnapshots();
	}

	// Set mode to reverse edit (the grid shows the start state)
	CurrentMode = EPuzzleEditorMode::ReverseEdit;

	UE_LOG(LogTemp, Log, TEXT("PuzzleEditor: MCTS auto-generated puzzle applied. "
		"Score=%.4f, Moves=%d, Grid=%dx%d"),
		Result.BestScore, Result.ForwardSolution.Num(),
		Result.StartState.GridSize.X, Result.StartState.GridSize.Y);

	return Result;
}

// ==================================================================
// Clear Grid
// ==================================================================

void UPuzzleEditorSubsystem::ClearGrid()
{
	UPushBoxGridManager* GM = GetActiveGridManager();
	if (!GM) return;

	const int32 SX = GM->GridSize.X;
	const int32 SY = GM->GridSize.Y;

	// Resize cells if needed
	GM->Cells.SetNum(SX * SY);

	// Fill: outer border = Wall, interior = Empty
	for (int32 Y = 0; Y < SY; ++Y)
	{
		for (int32 X = 0; X < SX; ++X)
		{
			FPushBoxCellData& Cell = GM->Cells[Y * SX + X];
			Cell = FPushBoxCellData(); // reset all fields

			if (X == 0 || X == SX - 1 || Y == 0 || Y == SY - 1)
				Cell.TileType = EPushBoxTileType::Wall;
			else
				Cell.TileType = EPushBoxTileType::Empty;
		}
	}

	// Clear boxes, targets, history
	GM->BoxPositions.Empty();
	GM->MoveHistory.Empty();
	GM->MoveCount = 0;
	GM->PlayerGridPosition = FIntPoint(SX / 2, SY / 2);

	// Clear goal snapshot
	GoalSnapshot = FPushBoxGridSnapshot();

	// Reset obstacle generator
	if (ObstacleGen)
	{
		ObstacleGen->ClearSnapshots();
	}

	// Reset to tile placement mode
	CurrentMode = EPuzzleEditorMode::TilePlacement;

	UE_LOG(LogTemp, Log, TEXT("PuzzleEditor: Grid cleared to %dx%d (border walls only)."), SX, SY);
}

void UPuzzleEditorSubsystem::ResizeGrid(FIntPoint NewSize)
{
	NewSize.X = FMath::Clamp(NewSize.X, 3, 32);
	NewSize.Y = FMath::Clamp(NewSize.Y, 3, 32);

	UPushBoxGridManager* GM = GetActiveGridManager();
	if (!GM)
	{
		EditorGridManager = NewObject<UPushBoxGridManager>(this);
		GM = EditorGridManager;
	}

	GM->GridSize = NewSize;
	ClearGrid();

	SyncWFCGridSize(NewSize);

	UE_LOG(LogTemp, Log, TEXT("PuzzleEditor: Grid resized to %dx%d."), NewSize.X, NewSize.Y);
}

// ==================================================================
// WFC Asset Sync
// ==================================================================

void UPuzzleEditorSubsystem::SetWFCAsset(UWFCAsset* InWFCAsset)
{
	if (!ActivePuzzleData)
		return;

	ActivePuzzleData->WFCAssetRef = InWFCAsset;
	ActivePuzzleData->MarkPackageDirty();

	// If a WFC asset is assigned, sync its grid dimensions to match the puzzle
	if (InWFCAsset)
	{
		UPushBoxGridManager* GM = GetActiveGridManager();
		FIntPoint GridSize = GM ? GM->GridSize : ActivePuzzleData->GridSize;
		if (GridSize.X > 0 && GridSize.Y > 0)
		{
			SyncWFCGridSize(GridSize);
		}
	}
}

void UPuzzleEditorSubsystem::SyncWFCGridSize(FIntPoint NewGridSize)
{
	if (!ActivePuzzleData)
		return;

	UWFCAsset* WFCAsset = ActivePuzzleData->WFCAssetRef.LoadSynchronous();
	if (!WFCAsset || !WFCAsset->GridConfig)
		return;

	bool bModified = false;

	// Try 2D config first
	if (UWFCGrid2DConfig* Config2D = Cast<UWFCGrid2DConfig>(WFCAsset->GridConfig))
	{
		if (Config2D->Dimensions != NewGridSize)
		{
			Config2D->Dimensions = NewGridSize;
			bModified = true;
		}
	}

	if (bModified)
	{
		// Mark dirty and save the WFC asset
		WFCAsset->MarkPackageDirty();

		UPackage* Package = WFCAsset->GetOutermost();
		FString PackageFilename;
		if (FPackageName::TryConvertLongPackageNameToFilename(
				Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
		{
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			UPackage::SavePackage(Package, WFCAsset, *PackageFilename, SaveArgs);
		}

		UE_LOG(LogTemp, Log, TEXT("PuzzleEditor: Synced WFC asset '%s' grid dimensions to (%d, %d)"),
			*WFCAsset->GetName(), NewGridSize.X, NewGridSize.Y);
	}
}
