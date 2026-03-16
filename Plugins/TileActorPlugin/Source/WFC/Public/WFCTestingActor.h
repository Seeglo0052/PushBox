// Copyright Bohdon Sayre. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/WFCTypes.h"
#include "WFCTestingActor.generated.h"

class UWFCGeneratorComponent;
struct FWFCModelAssetTile;

/**
 * Test actor for working with a WFC Generator component.
 */
UCLASS(BlueprintType, Blueprintable)
class WFC_API AWFCTestingActor : public AActor
{
	GENERATED_BODY()

public:
	AWFCTestingActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WFC")
	TObjectPtr<UWFCGeneratorComponent> WFCGenerator;

	/** Load tile levels when generation finishes successfully. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
	bool bLoadTileLevelsOnFinish;

	/** Spawn tile actors even when generation fails. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
	bool bSpawnOnError;

	/** Spawn tile actors immediately as cells are selected during generation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
	bool bSpawnOnCellSelection;

	/** Load tile levels when spawning during cell selection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
	bool bLoadTileLevelsOnSelection;

	/** Enable multiplayer synchronization features */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Multiplayer")
	bool bEnableMultiplayer;

	/** If true, only the server will run generation and clients will receive the result */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Multiplayer", meta = (EditCondition = "bEnableMultiplayer"))
	bool bServerOnlyGeneration;

	/** Delay before server starts generation to allow clients to connect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Multiplayer", meta = (EditCondition = "bEnableMultiplayer"))
	float ServerGenerationDelay;

	/** Current WFC seed being used for generation. Updated when RunNewGenerationServerOnly generates a random seed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WFC")
	int32 WFC_Seed = 1;

	/** Get the asset tile for the selected tile in a cell. */
	const FWFCModelAssetTile* GetAssetTileForCell(int32 CellIndex) const;

	/** Get the world transform for a cell and rotation. */
	FTransform GetCellTransform(int32 CellIndex, int32 Rotation = 0) const;

	/** AActor */
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

	/** Configure multiplayer settings for this actor */
	UFUNCTION(BlueprintCallable, Category = "Multiplayer")
	void ConfigureMultiplayer(bool bEnable, bool bRequestFromServer = true);

	/** Server-side function to start generation with a specific seed */
	UFUNCTION(BlueprintCallable, Category = "Multiplayer", meta = (CallInEditor = "true"))
	void ServerStartGeneration(int32 Seed = 0);

	/** Get the current random seed being used */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WFC")
	int32 GetCurrentSeed() const;

	/** Start a new generation (clears previous results) */
	UFUNCTION(BlueprintCallable, Category = "WFC", meta = (CallInEditor = "true"))
	void RunNewGeneration(int32 NewSeed = 0);

	/**
	 * Start a new generation (server-only mode for iterative fixing).
	 * This skips client notification - use SyncFinalGenerationToClients when ready.
	 * 
	 * @param NewSeed Random seed (0 = generate new)
	 * @param bSpawnActors If true, spawn actors after generation; if false, only virtual computation
	 */
	UFUNCTION(BlueprintCallable, Category = "WFC|Multiplayer")
	void RunNewGenerationServerOnly(int32 NewSeed = 0, bool bSpawnActors = false);

	/**
	 * Sync the final successful generation to all clients.
	 * Call this after iterative fixing succeeds to replicate the result.
	 * Sends: Final Seed + All FixedTileMappings + Start generation command.
	 */
	UFUNCTION(BlueprintCallable, Category = "WFC|Multiplayer")
	void SyncFinalGenerationToClients();

	/**
	 * Clear all dynamic fixed tiles from iterative fixing.
	 * Call this before starting a new iterative fixing session to avoid accumulation.
	 * 
	 * ⚠️ MULTIPLAYER: Only call this on SERVER! Clients will receive sync automatically.
	 */
	UFUNCTION(BlueprintCallable, Category = "WFC|Multiplayer")
	void ClearDynamicFixedTiles();

	/**
	 * IMPROVED: Clear and synchronize dynamic fixed tiles with confirmation.
	 * This ensures all clients receive and apply the clear command before proceeding.
	 * 
	 * ⚠️ SERVER ONLY: Call this on server before starting new iterative session.
	 * 
	 * @return True if successfully cleared and synced to clients
	 */
	UFUNCTION(BlueprintCallable, Category = "WFC|Multiplayer")
	bool ClearAndSyncDynamicFixedTiles();

	/**
	 * Debug function: Print current generation state for both server and client.
	 * Useful for diagnosing multiplayer sync issues.
	 */
	UFUNCTION(BlueprintCallable, Category = "WFC|Debug")
	void PrintGenerationState();

	/**
	 * Export all fixed tiles (default + dynamic) for host migration or save/load.
	 * Returns a lightweight array of fixed tile mappings that can be saved.
	 * 
	 * USAGE:
	 *   - After successful iterative fixing
	 *   - Before host migration
	 *   - For save/load system
	 * 
	 * @param OutFixedTiles Array of all fixed tiles (both static and dynamic)
	 * @return Number of fixed tiles exported
	 */
	UFUNCTION(BlueprintCallable, Category = "WFC|Migration")
	int32 ExportAllFixedTiles(TArray<FWFCFixedTileData>& OutFixedTiles) const;

	/**
	 * Import fixed tiles and apply them to the generator.
	 * Use this after host migration or load to restore the constraint state.
	 * 
	 * USAGE:
	 *   - After host migration (before generation)
	 *   - On load (to restore saved state)
	 * 
	 * @param FixedTiles Array of fixed tiles to import
	 * @return True if successfully imported and applied
	 */
	UFUNCTION(BlueprintCallable, Category = "WFC|Migration")
	bool ImportAndApplyFixedTiles(const TArray<FWFCFixedTileData>& FixedTiles);

	/**
	 * Cleanup unreachable tiles to optimize performance.
	 * Uses BFS to find reachable region from start location, then destroys actors
	 * in unreachable cells. This is a Multicast RPC - all machines execute locally.
	 * 
	 * USAGE: Call this AFTER successful generation to remove unreachable areas.
	 * 
	 * @param StartLocation Starting point for BFS (e.g., player spawn point)
	 * @param PassableEdgeTags Tags that define passable connections (from PathfindingConstraint)
	 * @param RetainTags If any tile actor has these tags, skip deletion (retain important tiles)
	 */
	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = "WFC|Optimization")
	void MulticastCleanupUnreachableTiles(FIntVector StartLocation, const FGameplayTagContainer& PassableEdgeTags, const FGameplayTagContainer& RetainTags);

	/** Destroy all spawned tile actors. */
	UFUNCTION(BlueprintCallable, Category = "WFC")
	void DestroyAllSpawnedActors();

	/** Server-side function to start deterministic generation with a specific seed */
	UFUNCTION(Server, Reliable, WithValidation, Category = "Multiplayer")
	void ServerRequestDeterministicGeneration(int32 Seed);

	/** Server starts deterministic generation and notifies all clients */
	void ServerStartDeterministicGeneration(int32 Seed);

	/** Multicast RPC to start deterministic generation on all clients */
	UFUNCTION(NetMulticast, Reliable, Category = "Multiplayer")
	void MulticastStartDeterministicGeneration(int32 Seed);

	/**
	 * Test deterministic generation by running the same seed multiple times.
	 * This function can be used to verify that the same seed always produces the same result.
	 */
	UFUNCTION(BlueprintCallable, Category = "WFC", meta = (CallInEditor = "true"))
	void TestDeterministicGeneration(int32 TestSeed = 12345, int32 TestIterations = 3);

	/**
	 * Validate that the deterministic generation is working correctly.
	 * Tests the same seed multiple times to ensure identical results.
	 */
	UFUNCTION(BlueprintCallable, Category = "WFC", meta = (CallInEditor = "true"))
	bool ValidateDeterministicGeneration(int32 TestSeed, int32 TestIterations = 3);

	/** 
	 * Get a hash of the current generation result for comparison.
	 * Useful for verifying that different seeds produce different results.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "WFC")
	int32 GetGenerationResultHash() const;

protected:
	/** Timer handle for delayed server generation */
	FTimerHandle ServerGenerationTimerHandle;

	/** Called when server generation delay expires */
	void DelayedServerGeneration();

	/** Called when a cell is selected during generation. */
	UFUNCTION()
	void OnCellSelected(int32 CellIndex);

	/** Called when the generator has finished running. */
	UFUNCTION()
	void OnGeneratorFinished(EWFCGeneratorState State);

	/** Called when a seed is received from the server */
	UFUNCTION()
	void OnSeedReceived(int32 Seed);

	/** Spawn an actor for the selected tile at the given cell index. */
	AActor* SpawnActorForCell(int32 CellIndex, bool bAutoLoad = false);

	/** Get the spawned actor for a cell, if any. */
	AActor* GetActorForCell(int32 CellIndex) const;

	/** Spawn actors for all cells that have been selected. */
	void SpawnActorsForAllCells();

	/** Load all spawned tile actor levels. */
	void LoadAllTileActorLevels();

	/** Get a string representation of the current network role for logging */
	const TCHAR* GetNetworkRoleString() const
	{
		if (!GetWorld()) return TEXT("UNKNOWN");
		switch (GetWorld()->GetNetMode())
		{
			case NM_Standalone: return TEXT("STANDALONE");
			case NM_DedicatedServer: return TEXT("SERVER");
			case NM_ListenServer: return HasAuthority() ? TEXT("LISTEN_SERVER") : TEXT("CLIENT");
			case NM_Client: return TEXT("CLIENT");
			default: return TEXT("UNKNOWN");
		}
	}

private:
	/** Map of spawned tile actors by cell index. */
	TMap<int32, TWeakObjectPtr<AActor>> SpawnedTileActors;

	/** Cache for validation results to avoid repeated allocations */
	mutable TArray<int32> TempResultsArray;
	
	/** Internal flag to control whether OnGeneratorFinished should spawn actors */
	bool bShouldSpawnActorsOnFinish = true;

	/** Helper function to generate a valid random seed */
	int32 GenerateValidSeed(int32 InputSeed = 0) const;

	/** Helper function to wait for WFC generation completion */
	bool WaitForGenerationCompletion(int32 TimeoutMs = 10000) const;

	/** Helper function to run deterministic generation test core logic */
	bool RunDeterministicGenerationCore(int32 TestSeed, int32 TestIterations, bool bSpawnActorsOnCompletion = false);
};
