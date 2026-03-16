// Copyright Bohdon Sayre. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Core/WFCTypes.h"
#include "Core/Constraints/WFCFixedTileConstraint.h"
#include "Math/RandomStream.h"
#include "Engine/Engine.h"
#include "Net/UnrealNetwork.h"
#include "WFCGeneratorComponent.generated.h"

class UWFCAsset;
class UWFCGenerator;
class UWFCGrid;


/**
 * A component that handles running a Wave Function Collapse generator,
 * and then populating the scene with tile actors as desired.
 */
UCLASS(ClassGroup=(Procedural), meta=(BlueprintSpawnableComponent))
class WFC_API UWFCGeneratorComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UWFCGeneratorComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UWFCAsset> WFCAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 StepLimit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAutoRun;

	// Multiplayer Support
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Multiplayer")
	bool bUseMultiplayerSeeds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Multiplayer", meta = (EditCondition = "bUseMultiplayerSeeds"))
	bool bRequestSeedFromServer;

	UPROPERTY(ReplicatedUsing = OnRep_RandomSeed)
	int32 RandomSeed;

	bool bWaitingForSeed;

	FRandomStream RandomStream;

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	UFUNCTION(BlueprintCallable)
	bool Initialize(bool bForce = false);

	UFUNCTION(BlueprintPure)
	bool IsInitialized() const;

	UFUNCTION(BlueprintCallable)
	void ResetGenerator();

	UFUNCTION(BlueprintCallable)
	void Run();

	UFUNCTION(BlueprintCallable)
	void SetSeedAndRun(int32 Seed);

	UFUNCTION(BlueprintPure, Category = "WFC")
	int32 GetRandomSeed() const;

	UFUNCTION(BlueprintPure)
	const UWFCGrid* GetGrid() const;

	UFUNCTION(BlueprintPure)
	EWFCGeneratorState GetState() const;

	UFUNCTION(BlueprintCallable, BlueprintPure = false)
	void GetSelectedTileId(int32 CellIndex, bool& bSuccess, int32& TileId) const;

	UFUNCTION(BlueprintCallable, BlueprintPure = false)
	void GetSelectedTileIds(TArray<int32>& TileIds) const;

	// Multiplayer RPC Functions
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestSeed();

	UFUNCTION(Client, Reliable)
	void ClientReceiveSeed(int32 Seed);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestGeneration();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastStartGeneration(int32 Seed);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastSyncFixedTiles(const TArray<FWFCFixedTileData>& FixedTiles);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastSyncAndGenerate(int32 Seed, const TArray<FWFCFixedTileData>& FixedTiles);

private:
	UFUNCTION()
	void OnRep_RandomSeed();

	DECLARE_MULTICAST_DELEGATE_OneParam(FCellSelectedDelegate, int32 /*CellIndex*/);

public:
	FCellSelectedDelegate OnCellSelectedEvent;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCellSelectedDynDelegate, int32, CellIndex);

	UPROPERTY(BlueprintAssignable)
	FCellSelectedDynDelegate OnCellSelectedEvent_BP;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStateChangedDynDelegate, EWFCGeneratorState, State);

	UPROPERTY(BlueprintAssignable)
	FStateChangedDynDelegate OnStateChangedEvent_BP;

	DECLARE_MULTICAST_DELEGATE_OneParam(FFinishedDelegate, EWFCGeneratorState /*State*/)

	FFinishedDelegate OnFinishedEvent;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFinishedDynDelegate, bool, bSuccess);

	UPROPERTY(BlueprintAssignable)
	FFinishedDynDelegate OnFinishedEvent_BP;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSeedReceivedDynDelegate, int32, Seed);

	UPROPERTY(BlueprintAssignable)
	FSeedReceivedDynDelegate OnSeedReceivedEvent_BP;

	UWFCGenerator* GetGenerator() const { return Generator; }

protected:
	UPROPERTY(Transient, BlueprintReadOnly)
	TObjectPtr<UWFCGenerator> Generator = nullptr;

	void OnCellSelected(int32 CellIndex);
	void OnStateChanged(EWFCGeneratorState State);

	int32 GenerateNewSeed();
	void StartGeneration();
	void HandleAutoRun();

private:
	void PrepareClientForGeneration(int32 Seed);
};
