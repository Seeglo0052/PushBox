// Copyright Bohdon Sayre. All Rights Reserved.


#include "Core/Constraints/WFCFixedTileConstraint.h"

#include "WFCModule.h"
#include "Core/WFCGenerator.h"


void UWFCFixedTileConstraint::Initialize(UWFCGenerator* InGenerator)
{
	Super::Initialize(InGenerator);
}

void UWFCFixedTileConstraint::Reset()
{
	Super::Reset();
	bDidApplyInitialConstraint = false;
}

void UWFCFixedTileConstraint::AddFixedTileMapping(FWFCCellIndex CellIndex, FWFCTileId TileId)
{
	FixedTileMappings.Add(FWFCFixedTileConstraintEntry(CellIndex, TileId));
}

bool UWFCFixedTileConstraint::Next()
{
	if (bDidApplyInitialConstraint)
	{
		return false;
	}

	if (FixedTileMappings.IsEmpty())
	{
		bDidApplyInitialConstraint = true;
		return false;
	}

	bool bDidMakeChanges = false;
	for (const FWFCFixedTileConstraintEntry& TileMapping : FixedTileMappings)
	{
		if (!Generator->IsValidCellIndex(TileMapping.CellIndex)) continue;
		if (!Generator->IsValidTileId(TileMapping.TileId)) continue;
		Generator->Select(TileMapping.CellIndex, TileMapping.TileId);
		bDidMakeChanges = true;
	}

	bDidApplyInitialConstraint = true;
	return bDidMakeChanges;
}

void UWFCFixedTileConstraint::SetFixedTileMapping(FWFCCellIndex CellIndex, FWFCTileId TileId)
{
	for (FWFCFixedTileConstraintEntry& Entry : FixedTileMappings)
	{
		if (Entry.CellIndex == CellIndex)
		{
			Entry.TileId = TileId;
			return;
		}
	}
	FixedTileMappings.Add(FWFCFixedTileConstraintEntry(CellIndex, TileId, true));
}

const FWFCTileId* UWFCFixedTileConstraint::GetFixedTileMapping(FWFCCellIndex CellIndex) const
{
	for (const FWFCFixedTileConstraintEntry& Entry : FixedTileMappings)
	{
		if (Entry.CellIndex == CellIndex)
		{
			return &Entry.TileId;
		}
	}
	return nullptr;
}
