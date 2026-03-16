// Copyright PushBox Game. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PushBoxTypes.h"
#include "PuzzleLevelDataAsset.generated.h"

class UWFCAsset;

/**
 * Data asset that stores a complete puzzle level definition.
 * Created by the editor tool (reverse-design workflow), loaded at runtime for gameplay.
 *
 * Workflow:
 * 1. Designer creates this asset via PushBox Editor tab
 * 2. Terrain is generated (walls/floors/ice/doors) via the tile system or manual placement
 * 3. Designer places boxes on targets (goal state), then reverse-pulls to define start
 * 4. Both snapshots are saved here
 * 5. At runtime, StartState is loaded for the player to solve forward
 */
UCLASS(BlueprintType)
class PUSHBOX_API UPuzzleLevelDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Display name shown in level select UI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
	FText PuzzleName;

	/** Designer notes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle", meta = (MultiLine = true))
	FText DesignerNotes;

	/** Difficulty rating 1-5 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle", meta = (ClampMin = 1, ClampMax = 5))
	int32 Difficulty = 1;

	/** Grid dimensions */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	FIntPoint GridSize = FIntPoint(10, 10);

	/** WFC asset used to generate the terrain base */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC")
	TSoftObjectPtr<UWFCAsset> WFCAssetRef;

	/** WFC seed used to generate this level (for reproducibility) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC")
	int32 WFCSeed = 0;

	/** The START state (what the player sees when the level loads) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	FPushBoxGridSnapshot StartState;

	/** The GOAL state (all boxes on their targets - the win condition) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	FPushBoxGridSnapshot GoalState;

	/** The solution move sequence (reverse of the designer's pull sequence) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solution")
	TArray<FPushBoxMoveRecord> SolutionMoves;

	/** Par move count (optimal / designer-intended solution length) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solution")
	int32 ParMoveCount = 0;

	/** Validate that start and goal states are consistent */
	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	bool ValidatePuzzle(FText& OutError) const;

	/** Get the number of boxes in this puzzle */
	UFUNCTION(BlueprintPure, Category = "Puzzle")
	int32 GetBoxCount() const { return GoalState.BoxPositions.Num(); }

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId("PuzzleLevel", GetFName());
	}
};
