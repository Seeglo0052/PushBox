// Copyright Bohdon Sayre. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DynamicLevelInstance.h"
#include "WFCTileActorInterface.h"
#include "WFCTileAsset.h"
#include "Core/WFCTypes.h"
#include "WFCTileLevelInstance.generated.h"

class UWFCGeneratorComponent;


UCLASS()
class WFC_API AWFCTileLevelInstance : public ADynamicLevelInstance,
                                      public IWFCTileActorInterface
{
	GENERATED_BODY()

public:
	AWFCTileLevelInstance();

	/** IWFCTileActorInterface */
	virtual void SetGeneratorComp(UWFCGeneratorComponent* NewGeneratorComp) override;
	virtual void SetTileAndCell(const FWFCModelTile* NewTile, FWFCCellIndex NewCellIndex) override;

	const FWFCModelAssetTile& GetModelTile() const { return ModelTile; }

protected:
	UPROPERTY(Transient, BlueprintReadOnly)
	TWeakObjectPtr<UWFCGeneratorComponent> GeneratorComp;

	UPROPERTY(Transient, BlueprintReadOnly)
	FWFCModelAssetTile ModelTile;

	UPROPERTY(Transient, BlueprintReadOnly)
	int32 CellIndex;
};
