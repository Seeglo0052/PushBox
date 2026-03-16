// Copyright Bohdon Sayre. All Rights Reserved.

#include "Core/WFCGenerator.h"

#include "WFCModule.h"
#include "Core/WFCConstraint.h"
#include "Core/Constraints/WFCFixedTileConstraint.h"
#include "Core/WFCGrid.h"
#include "Core/WFCModel.h"


UWFCGenerator::UWFCGenerator()
	: State(EWFCGeneratorState::None),
	  NumTiles(0),
	  NumCells(0),
	  NumUnselectedCells(0),
	  bIsInitialized(false),
	  bUseCustomRandomStream(false),
	  bForceDeterministic(false)
{
	RandomStream.Initialize(FMath::Rand());
}

void UWFCGenerator::SetState(EWFCGeneratorState NewState)
{
	if (State != NewState)
	{
		State = NewState;
		OnStateChanged.Broadcast(State);
	}
}

void UWFCGenerator::SetRandomSeed(int32 Seed)
{
	RandomStream.Initialize(Seed);
	bUseCustomRandomStream = true;
}

void UWFCGenerator::SetRandomStream(const FRandomStream& InRandomStream)
{
	RandomStream = InRandomStream;
	bUseCustomRandomStream = true;
	bForceDeterministic = true;
}

UWFCConstraint* UWFCGenerator::GetConstraint(TSubclassOf<UWFCConstraint> ConstraintClass) const
{
	for (UWFCConstraint* Constraint : Constraints)
	{
		if (Constraint->IsA(ConstraintClass))
		{
			return Constraint;
		}
	}
	return nullptr;
}

void UWFCGenerator::Configure(FWFCGeneratorConfig InConfig)
{
	Config = InConfig;
}

void UWFCGenerator::Initialize(bool bFull)
{
	if (bIsInitialized)
	{
		return;
	}

	Config.Model->GenerateTiles();
	NumTiles = Config.Model->GetNumTiles();

	InitializeGrid(Config.GridConfig.Get());
	InitializeCells();
	CreateConstraints();

	if (bFull)
	{
		InitializeConstraints();
	}

	SetState(EWFCGeneratorState::InProgress);
	bIsInitialized = true;
}

void UWFCGenerator::InitializeGrid(const UWFCGridConfig* GridConfig)
{
	check(GridConfig != nullptr);
	Grid = UWFCGrid::NewGrid(this, GridConfig);
	check(Grid != nullptr);
}

void UWFCGenerator::CreateConstraints()
{
	Constraints.Reset();
	for (const TSubclassOf<UWFCConstraint>& ConstraintClass : Config.ConstraintClasses)
	{
		if (!ConstraintClass) continue;
		UWFCConstraint* Constraint = NewObject<UWFCConstraint>(this, ConstraintClass);
		check(Constraint != nullptr);
		Constraints.Add(Constraint);
	}
}

void UWFCGenerator::InitializeConstraints()
{
	for (UWFCConstraint* Constraint : Constraints)
	{
		Constraint->Initialize(this);
	}
}

void UWFCGenerator::InitializeCells()
{
	TArray<FWFCTileId> AllTileCandidates;
	AllTileCandidates.SetNum(GetNumTiles());
	for (int32 Idx = 0; Idx < AllTileCandidates.Num(); ++Idx)
	{
		AllTileCandidates[Idx] = Idx;
	}

	NumCells = Grid->GetNumCells();
	NumUnselectedCells = NumCells;

	Cells.SetNum(NumCells);
	for (FWFCCellIndex Idx = 0; Idx < NumCells; ++Idx)
	{
		Cells[Idx].TileCandidates = AllTileCandidates;
	}
}

void UWFCGenerator::Reset()
{
	InitializeCells();

	for (UWFCConstraint* Constraint : Constraints)
	{
		Constraint->Reset();
	}

	CellsAffectedThisUpdate.Reset();
	SetState(EWFCGeneratorState::None);
}

void UWFCGenerator::Run(int32 StepLimit)
{
	if (!bIsInitialized)
	{
		UE_LOG(LogWFC, Error, TEXT("Initialize must be called before Run"));
		return;
	}

	if (State == EWFCGeneratorState::Finished || State == EWFCGeneratorState::Error)
	{
		Reset();
	}

	// Direct assignment: assign all fixed tiles and finish
	if (TryDirectAssignment())
	{
		return;
	}

	// Fallback: run constraints once
	for (UWFCConstraint* Constraint : Constraints)
	{
		Constraint->Next();
		if (State == EWFCGeneratorState::Finished || State == EWFCGeneratorState::Error)
		{
			return;
		}
	}

	if (NumUnselectedCells > 0)
	{
		UE_LOG(LogWFC, Warning, TEXT("Run: Not all cells were assigned. %d remain."), NumUnselectedCells);
		SetState(EWFCGeneratorState::Error);
	}
}

bool UWFCGenerator::TryDirectAssignment()
{
	UWFCFixedTileConstraint* FixedConstraint = GetConstraint<UWFCFixedTileConstraint>();
	if (!FixedConstraint) return false;

	const TArray<FWFCFixedTileConstraintEntry>& Mappings = FixedConstraint->GetFixedTileMappings();
	if (Mappings.Num() < NumCells) return false;

	TMap<FWFCCellIndex, FWFCTileId> CellToTile;
	CellToTile.Reserve(NumCells);
	for (const FWFCFixedTileConstraintEntry& Entry : Mappings)
	{
		if (IsValidCellIndex(Entry.CellIndex) && IsValidTileId(Entry.TileId))
		{
			CellToTile.Add(Entry.CellIndex, Entry.TileId);
		}
	}

	if (CellToTile.Num() < NumCells) return false;

	CellsAffectedThisUpdate.Reset();
	for (auto& Pair : CellToTile)
	{
		FWFCCell& Cell = GetCell(Pair.Key);
		Cell.TileCandidates.Reset(1);
		Cell.TileCandidates.Add(Pair.Value);
		CellsAffectedThisUpdate.AddUnique(Pair.Key);
		OnCellSelected.Broadcast(Pair.Key);
	}

	NumUnselectedCells = 0;
	SetState(EWFCGeneratorState::Finished);
	return true;
}

void UWFCGenerator::Select(int32 CellIndex, int32 TileId)
{
	if (IsValidCellIndex(CellIndex))
	{
		FWFCCell& Cell = GetCell(CellIndex);
		Cell.TileCandidates.Reset(1);
		Cell.TileCandidates.Add(TileId);
		OnCellChanged(CellIndex);
	}
}

FWFCCell& UWFCGenerator::GetCell(FWFCCellIndex CellIndex)
{
	return Cells[CellIndex];
}

const FWFCCell& UWFCGenerator::GetCell(FWFCCellIndex CellIndex) const
{
	return Cells[CellIndex];
}

int32 UWFCGenerator::GetNumCellCandidates(int32 CellIndex) const
{
	if (IsValidCellIndex(CellIndex))
	{
		return GetCell(CellIndex).TileCandidates.Num();
	}
	return 0;
}

FString UWFCGenerator::GetTileDebugString(int32 TileId) const
{
	if (Config.Model.IsValid())
	{
		if (const FWFCModelTile* ModelTile = Config.Model->GetTile(TileId))
		{
			return ModelTile->ToString();
		}
	}
	return FString();
}

void UWFCGenerator::GetSelectedTileIds(TArray<int32>& OutTileIds) const
{
	OutTileIds.SetNum(NumCells);
	for (FWFCCellIndex Idx = 0; Idx < NumCells; ++Idx)
	{
		OutTileIds[Idx] = GetCell(Idx).GetSelectedTileId();
	}
}

void UWFCGenerator::OnCellChanged(FWFCCellIndex CellIndex)
{
	CellsAffectedThisUpdate.AddUnique(CellIndex);

	FWFCCell& Cell = GetCell(CellIndex);
	if (Cell.HasSelection())
	{
		OnCellSelected.Broadcast(CellIndex);
		--NumUnselectedCells;
	}
	else if (Cell.HasNoCandidates())
	{
		SetState(EWFCGeneratorState::Error);
	}

	if (NumUnselectedCells == 0)
	{
		SetState(EWFCGeneratorState::Finished);
	}
}
