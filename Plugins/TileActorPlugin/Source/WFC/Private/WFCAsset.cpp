// Copyright Bohdon Sayre. All Rights Reserved.


#include "WFCAsset.h"

#include "WFCAssetModel.h"
#include "WFCTileSet.h"
#include "Core/WFCGenerator.h"
#include "Misc/DataValidation.h"
#include "UObject/ObjectSaveContext.h"

#define LOCTEXT_NAMESPACE "WFCEditor"


UWFCAsset::UWFCAsset()
{
	GeneratorClass = UWFCGenerator::StaticClass();
	ModelClass = UWFCAssetModel::StaticClass();
}

#if WITH_EDITOR
void UWFCAsset::PreSave(FObjectPreSaveContext SaveContext)
{
	Super::PreSave(SaveContext);

	if (!SaveContext.IsProceduralSave())
	{
		// remove invalid entries
		TileSets.RemoveAll([](const UWFCTileSet* TileSet)
		{
			return TileSet == nullptr;
		});

		// sort tile sets by name
		TileSets.Sort([](const UWFCTileSet& TileSetA, const UWFCTileSet& TileSetB)
		{
			return TileSetA.GetName() < TileSetB.GetName();
		});
	}
}

EDataValidationResult UWFCAsset::IsDataValid(FDataValidationContext& Context)
{
	EDataValidationResult Result = EDataValidationResult::Valid;

	if (TileSets.IsEmpty())
	{
		Context.AddError(LOCTEXT("NoTileSets", "No tile sets."));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
