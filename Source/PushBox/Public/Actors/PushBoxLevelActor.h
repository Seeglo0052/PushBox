// Copyright PushBox Game. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WFCTestingActor.h"
#include "PushBoxTypes.h"
#include "PushBoxLevelActor.generated.h"

class UPushBoxGridManager;
class UPuzzleLevelDataAsset;
class APushBoxBox;
class APushBoxTileActorBase;

/**
 * The main puzzle level actor. Inherits from AWFCTestingActor which provides
 * the grid-based tile spawning system (grid config, tile model, actor spawning).
 *
 * This actor adds:
 * - GridManager component for puzzle logic (cell types, box positions, win condition)
 * - Spawning of box actors and target markers
 * - Loading from PuzzleLevelDataAsset (forces grid cells to match saved layout)
 *
 * When a valid DataAsset is assigned:
 *   The tile system initializes (so tile assets and actor classes are available),
 *   every cell is fixed to match the DataAsset, and tile actors are spawned
 *   through the normal pipeline.
 */
UCLASS(BlueprintType, Blueprintable)
class PUSHBOX_API APushBoxLevelActor : public AWFCTestingActor
{
	GENERATED_BODY()

public:
	APushBoxLevelActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PushBox")
	TObjectPtr<UPushBoxGridManager> GridManager;

	/** Puzzle data asset to load (if set, forces grid cells to match saved layout) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PushBox")
	TObjectPtr<UPuzzleLevelDataAsset> PuzzleLevelData;

	/** Actor class to spawn for boxes (assign your BP_PushBoxBox here) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PushBox|Spawning")
	TSubclassOf<APushBoxBox> BoxActorClass;

	/** Actor class to spawn for target markers */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PushBox|Spawning")
	TSubclassOf<AActor> TargetMarkerClass;

	UFUNCTION(BlueprintCallable, Category = "PushBox", meta = (CallInEditor = "true"))
	void SetupPuzzle();

	UFUNCTION(BlueprintCallable, Category = "PushBox", meta = (CallInEditor = "true"))
	void GenerateAndSetup(int32 Seed = 0);

	UFUNCTION(BlueprintCallable, Category = "PushBox")
	void SpawnPuzzleActors();

	UFUNCTION(BlueprintCallable, Category = "PushBox")
	void ClearPuzzleActors();

	/** Refresh the level from PuzzleLevelData: re-init GridManager, rebuild WFC tiles,
	 *  reconfigure tile actors, and re-spawn puzzle actors.
	 *  Call this after the DataAsset has been updated (e.g., after editor Save). */
	UFUNCTION(BlueprintCallable, Category = "PushBox", meta = (CallInEditor = "true"))
	void RefreshFromDataAsset();

	/** Returns true if this actor has a valid DataAsset with saved cell data */
	bool HasValidDataAsset() const;

	virtual void BeginPlay() override;

protected:
	/** Called when WFC generation finishes ¡ª bound to native OnFinishedEvent
	 *  so it fires AFTER the parent's OnGeneratorFinished (which spawns tile actors). */
	void OnWFCFinished(EWFCGeneratorState State);

	void OnWFCGenerationComplete();
	void MapWFCResultsToCells();

	/**
	 * Fix all grid cells to match the DataAsset cell layout, then run the generator.
	 * Every cell is pre-determined, so it collapses instantly and spawns the correct
	 * tile actors through the normal pipeline.
	 */
	void RunWFCWithFixedCellsFromDataAsset();

	/** Build a lookup from EPushBoxTileType ¡ú TileId using the model's tile OwnedTags */
	void BuildTileTypeLookup(TMap<EPushBoxTileType, int32>& OutLookup) const;

	/**
	 * After tile actors are spawned, iterate all of them and call InitializeTileData()
	 * on any APushBoxTileActorBase instances. This sets grid coord, cell data,
	 * and applies direction rotation for OneWayDoor tiles.
	 */
	void ConfigureSpawnedTileActors();

	/** Callback when GridManager broadcasts a box move ¡ª notifies tile actors */
	UFUNCTION()
	void HandleBoxMoved(FIntPoint From, FIntPoint To);

	/** Teleport the player pawn to the grid position stored in GridManager.
	 *  Called after WFC generation completes and puzzle actors are spawned. */
	void TeleportPlayerToGridPosition();

	/** Get the APushBoxTileActorBase at a grid coordinate (if any). */
	APushBoxTileActorBase* GetPushBoxTileActorAtCoord(FIntPoint Coord) const;

private:
	UPROPERTY()
	TArray<TWeakObjectPtr<APushBoxBox>> SpawnedBoxActors;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> SpawnedTargetActors;

	/** Cached lookup: grid coord ¡ú PushBox tile actor (populated by ConfigureSpawnedTileActors) */
	UPROPERTY()
	TMap<int32, TWeakObjectPtr<APushBoxTileActorBase>> TileActorsByCoordIndex;
};
