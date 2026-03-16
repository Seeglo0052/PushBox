// Copyright Bohdon Sayre. All Rights Reserved.


#include "LevelInstance/WFCTileLevelInstance.h"

#include "WFCGeneratorComponent.h"


AWFCTileLevelInstance::AWFCTileLevelInstance()
	: CellIndex(INDEX_NONE)
{
}

void AWFCTileLevelInstance::SetGeneratorComp(UWFCGeneratorComponent* NewGeneratorComp)
{
	GeneratorComp = NewGeneratorComp;
}

void AWFCTileLevelInstance::SetTileAndCell(const FWFCModelTile* NewTile, FWFCCellIndex NewCellIndex)
{
	CellIndex = NewCellIndex;

	if (const FWFCModelAssetTile* AssetTilePtr = static_cast<const FWFCModelAssetTile*>(NewTile))
	{
		ModelTile = *AssetTilePtr;
	}
}
