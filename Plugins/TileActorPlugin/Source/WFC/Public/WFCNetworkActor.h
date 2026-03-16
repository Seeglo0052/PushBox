// Copyright Bohdon Sayre. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WFCTestingActor.h"
#include "GameFramework/Actor.h"
#include "Engine/Engine.h"
#include "Net/UnrealNetwork.h"
#include "WFCNetworkActor.generated.h"

class UWFCGeneratorComponent;

/**
 * Network-aware WFC actor that handles multiplayer seed synchronization and generation.
 * This actor is designed to work properly in multiplayer environments with deterministic results.
 */
UCLASS(BlueprintType, Blueprintable)
class WFC_API AWFCNetworkActor : public AWFCTestingActor
{
	GENERATED_BODY()

public:
	AWFCNetworkActor(const FObjectInitializer& ObjectInitializer);

	// Network Settings
	/** Master seed used for all generation (replicated from server) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Network")
	int32 MasterSeed;

	/** If true, only server authority can start generation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
	bool bServerAuthorityOnly;

	/** Time to wait before starting generation (allows clients to connect) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
	float GenerationStartDelay;

	/** Maximum number of generation retries if contradiction occurs */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
	int32 MaxGenerationRetries;

	// Network Events
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSeedSynchronized, int32, Seed);
	UPROPERTY(BlueprintAssignable, Category = "Network")
	FOnSeedSynchronized OnSeedSynchronized;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGenerationStarted, int32, Seed, bool, bIsRetry);
	UPROPERTY(BlueprintAssignable, Category = "Network")
	FOnGenerationStarted OnGenerationStarted;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnGenerationCompleted, bool, bSuccess, int32, UsedSeed, int32, RetryCount);
	UPROPERTY(BlueprintAssignable, Category = "Network")
	FOnGenerationCompleted OnGenerationCompleted;

	// Network Methods
	/** Start network generation (server only) */
	UFUNCTION(BlueprintCallable, Category = "Network")
	void StartNetworkGeneration(int32 CustomSeed = 0);

	/** Request generation start from server (client to server) */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Network")
	void ServerRequestGenerationStart(int32 CustomSeed = 0);

	/** Broadcast generation start to all clients (server to clients) */
	UFUNCTION(NetMulticast, Reliable, Category = "Network")
	void MulticastStartGeneration(int32 Seed, bool bIsRetry = false);

	/** Broadcast generation completed to all clients (server to clients) */
	UFUNCTION(NetMulticast, Reliable, Category = "Network")
	void MulticastGenerationCompleted(bool bSuccess, int32 UsedSeed, int32 RetryCount);

	/** Get network statistics for debugging */
	UFUNCTION(BlueprintPure, Category = "Network")
	FString GetNetworkStatusString() const;

	// Overrides
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	/** Current retry count for failed generations */
	int32 CurrentRetryCount;

	/** Timer handle for delayed generation start */
	FTimerHandle GenerationDelayTimer;

	/** Called when MasterSeed is replicated */
	UFUNCTION()
	void OnRep_MasterSeed();

	/** Internal method to execute generation with current settings */
	void ExecuteGeneration(int32 Seed, bool bIsRetry = false);

	/** Handle generation completion and potential retries */
	void OnGeneratorFinished(EWFCGeneratorState State);

	/** Generate a new seed for retry attempts */
	int32 GenerateRetrySeed(int32 BaseSeed, int32 RetryIndex);

	/** Start generation after delay */
	void DelayedGenerationStart();

	/** Get the WFC Generator component (public accessor) */
	UWFCGeneratorComponent* GetWFCGenerator() const;
};