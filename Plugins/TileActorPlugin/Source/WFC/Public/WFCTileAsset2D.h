// Copyright Bohdon Sayre. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "WFCTileAsset.h"
#include "WFCTileAsset2D.generated.h"


/**
 * Definition of a tile within a group.
 */
USTRUCT(BlueprintType)
struct FWFCTileDef2D
{
	GENERATED_BODY()

	FWFCTileDef2D()
		: Location(FIntPoint::ZeroValue)
	{
	}

	/** The relative location of this tile within the group. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint Location;

	/** The actor to spawn for this tile */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AActor> ActorClass;
};


/**
 * A 2D tile asset that references an actor to be spawned for each tile.
 */
UCLASS()
class WFC_API UWFCTileAsset2D : public UWFCTileAsset
{
	GENERATED_BODY()

public:
	UWFCTileAsset2D();

	/** The dimensions of this tile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint Dimensions;

	/** Can this piece be rotated? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAllowRotation;

	/** The definitions for each tile within this asset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (TitleProperty = "Location"))
	TArray<FWFCTileDef2D> TileDefs;

	/** Return a tile def by location, as well as its index. */
	UFUNCTION(BlueprintPure)
	FWFCTileDef2D GetTileDefByLocation(FIntPoint Location, int32& Index) const;

	UFUNCTION(BlueprintPure)
	FWFCTileDef2D GetTileDefByIndex(int32 Index) const;

	virtual void GetAllowedRotations(TArray<int32>& OutRotations) const override;
	virtual int32 GetNumTileDefs() const override { return TileDefs.Num(); }
	virtual TSubclassOf<AActor> GetTileDefActorClass(int32 TileDefIndex) const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) override;
#endif
};
