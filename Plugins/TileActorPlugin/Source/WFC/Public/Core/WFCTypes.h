// Copyright Bohdon Sayre. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WFCTypes.generated.h"


/** Represents a direction pointing from one cell in a grid to another. */
typedef int32 FWFCGridDirection;

/** Represents the id of a cell within a grid. */
typedef int32 FWFCCellIndex;

/** Represents the id of a tile that can be selected for one cell of a grid. */
typedef int32 FWFCTileId;


UENUM(BlueprintType)
enum class EWFCGeneratorState : uint8
{
	/** Not yet started */
	None,
	/** A tile or tiles has not yet been selected. */
	InProgress,
	/** One or all tiles in question have been selected successfully. */
	Finished,
	/** A contradiction occured preventing the selection of a tile. */
	Error,
};


/**
 * Stores the state of a single cell within a grid during WFC generation.
 */
USTRUCT()
struct FWFCCell
{
	GENERATED_BODY()

	FWFCCell()
	{
	}

	/** The array of tile candidates for this cell. */
	UPROPERTY()
	TArray<int32> TileCandidates;

	FORCEINLINE bool HasNoCandidates() const { return TileCandidates.Num() == 0; }
	FORCEINLINE bool HasSelection() const { return TileCandidates.Num() == 1; }
	FORCEINLINE bool HasSelectionOrNoCandidates() const { return TileCandidates.Num() <= 1; }

	/** @return True if the candidates were changed */
	bool AddCandidate(FWFCTileId TileId);

	/** @return True if the candidates were changed */
	bool RemoveCandidate(FWFCTileId TileId);

	/** Return the selected tile id, or INDEX_NONE if not selected */
	FORCEINLINE FWFCTileId GetSelectedTileId() const { return TileCandidates.Num() == 1 ? TileCandidates[0] : INDEX_NONE; }

	/** Return true if any of the tile ids are a candidate for a cell. */
	bool HasAnyMatchingCandidate(const TArray<FWFCTileId>& TileIds) const;
};


/**
 * Contains all relevant data about a tile needed to select tiles
 * and generate final output from the model once selected.
 */
USTRUCT(BlueprintType)
struct FWFCModelTile
{
	GENERATED_BODY()

	FWFCModelTile()
		: Id(INDEX_NONE),
		  Weight(1.f)
	{
	}

	virtual ~FWFCModelTile()
	{
	}

	/** The id of this tile */
	UPROPERTY(BlueprintReadOnly)
	int32 Id;

	/** The probability weight of this tile */
	UPROPERTY(BlueprintReadOnly)
	float Weight;

	virtual FString ToString() const { return FString::Printf(TEXT("[%d]"), Id); }
};
