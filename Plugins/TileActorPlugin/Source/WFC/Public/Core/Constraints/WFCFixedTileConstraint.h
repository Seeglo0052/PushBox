// Copyright Bohdon Sayre. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/WFCConstraint.h"
#include "WFCFixedTileConstraint.generated.h"


/**
 * Lightweight structure for replicating fixed tile mappings over network.
 */
USTRUCT(BlueprintType)
struct FWFCFixedTileData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Fixed Tile Data")
	int32 CellIndex;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC|Fixed Tile Data")
	int32 TileId;
	
	FWFCFixedTileData() : CellIndex(INDEX_NONE), TileId(INDEX_NONE) {}
	FWFCFixedTileData(int32 InCell, int32 InTile) : CellIndex(InCell), TileId(InTile) {}
};


struct FWFCFixedTileConstraintEntry
{
	FWFCFixedTileConstraintEntry()
		: CellIndex(INDEX_NONE),
		  TileId(INDEX_NONE),
		  bIsDynamic(false)
	{
	}

	FWFCFixedTileConstraintEntry(FWFCCellIndex InCellIndex, FWFCTileId InTileId, bool bInIsDynamic = false)
		: CellIndex(InCellIndex),
		  TileId(InTileId),
		  bIsDynamic(bInIsDynamic)
	{
	}

	FWFCCellIndex CellIndex;
	FWFCTileId TileId;
	
	/** If true, this mapping was added dynamically at runtime (e.g., by iterative fixing) and should be preserved across resets */
	bool bIsDynamic;
};


/**
 * A constraint that specifies exact tiles that must be used for specific cells.
 */
UCLASS(Abstract)
class WFC_API UWFCFixedTileConstraint : public UWFCConstraint
{
	GENERATED_BODY()

public:
	virtual void Initialize(UWFCGenerator* InGenerator) override;
	virtual void Reset() override;
	virtual bool Next() override;

	/** Add a tile constraint to be applied next time this constraint runs. */
	void AddFixedTileMapping(FWFCCellIndex CellIndex, FWFCTileId TileId);

	/** Set a fixed tile mapping (replaces existing if present) */
	void SetFixedTileMapping(FWFCCellIndex CellIndex, FWFCTileId TileId);

	/** Get the fixed tile for a specific cell (returns nullptr if not fixed) */
	const FWFCTileId* GetFixedTileMapping(FWFCCellIndex CellIndex) const;

	/** Get total number of fixed tile mappings */
	int32 GetFixedTileMappingsCount() const { return FixedTileMappings.Num(); }

	/** Clear all fixed tile mappings (both static and dynamic) */
	void ClearFixedTileMappings() { FixedTileMappings.Reset(); }

	/** Clear only dynamic fixed tile mappings (preserves static configuration) */
	void ClearDynamicFixedTileMappings()
	{
		FixedTileMappings.RemoveAll([](const FWFCFixedTileConstraintEntry& Entry)
		{
			return Entry.bIsDynamic;
		});
	}

	/** Get all dynamic mappings for debugging/syncing */
	void GetDynamicMappings(TArray<FWFCFixedTileConstraintEntry>& OutDynamicMappings) const
	{
		OutDynamicMappings.Reset();
		for (const FWFCFixedTileConstraintEntry& Entry : FixedTileMappings)
		{
			if (Entry.bIsDynamic)
			{
				OutDynamicMappings.Add(Entry);
			}
		}
	}

	/** Get all fixed tile mappings (for fast-path direct assignment) */
	const TArray<FWFCFixedTileConstraintEntry>& GetFixedTileMappings() const { return FixedTileMappings; }

protected:
	TArray<FWFCFixedTileConstraintEntry> FixedTileMappings;

	bool bDidApplyInitialConstraint;
};

