// Copyright Bohdon Sayre. All Rights Reserved.

#include "WFCNetworkActor.h"
#include "WFCGeneratorComponent.h"
#include "WFCModule.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

AWFCNetworkActor::AWFCNetworkActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	  MasterSeed(0),
	  bServerAuthorityOnly(true),
	  GenerationStartDelay(2.0f),
	  MaxGenerationRetries(3),
	  CurrentRetryCount(0)
{
	// Ensure replication is enabled
	bReplicates = true;
	bAlwaysRelevant = true;
	
	// Disable base class multiplayer handling since we handle it here
	bEnableMultiplayer = false;
}

void AWFCNetworkActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AWFCNetworkActor, MasterSeed);
}

void AWFCNetworkActor::BeginPlay()
{
	// Configure WFC component for network operation
	UWFCGeneratorComponent* WFCComp = GetWFCGenerator();
	if (WFCComp)
	{
		WFCComp->bUseMultiplayerSeeds = true;
		WFCComp->bAutoRun = false; // We'll control when to run
	}

	Super::BeginPlay();

	// Server initializes generation after delay
	if (HasAuthority())
	{
		if (GenerationStartDelay > 0.0f)
		{
			GetWorld()->GetTimerManager().SetTimer(
				GenerationDelayTimer,
				this,
				&AWFCNetworkActor::DelayedGenerationStart,
				GenerationStartDelay,
				false
			);
		}
		else
		{
			DelayedGenerationStart();
		}
	}
}

void AWFCNetworkActor::DelayedGenerationStart()
{
	if (HasAuthority())
	{
		StartNetworkGeneration();
	}
}

void AWFCNetworkActor::StartNetworkGeneration(int32 CustomSeed)
{
	if (!HasAuthority())
	{
		UE_LOG(LogWFC, Warning, TEXT("StartNetworkGeneration called on non-authority"));
		return;
	}

	// Generate seed if not provided
	if (CustomSeed == 0)
	{
		CustomSeed = FMath::Rand();
		if (CustomSeed == 0) CustomSeed = 1; // Ensure non-zero
	}

	MasterSeed = CustomSeed;
	CurrentRetryCount = 0;

	UE_LOG(LogWFC, Log, TEXT("Server starting network generation with seed: %d"), MasterSeed);

	// Broadcast to all clients
	MulticastStartGeneration(MasterSeed, false);
}

void AWFCNetworkActor::ServerRequestGenerationStart_Implementation(int32 CustomSeed)
{
	if (bServerAuthorityOnly && !HasAuthority())
	{
		UE_LOG(LogWFC, Warning, TEXT("Client attempted to request generation start, but server authority only is enabled"));
		return;
	}

	StartNetworkGeneration(CustomSeed);
}

bool AWFCNetworkActor::ServerRequestGenerationStart_Validate(int32 CustomSeed)
{
	return true;
}

void AWFCNetworkActor::MulticastStartGeneration_Implementation(int32 Seed, bool bIsRetry)
{
	UE_LOG(LogWFC, Log, TEXT("Starting network generation: Seed=%d, IsRetry=%s, HasAuthority=%s"), 
		Seed, bIsRetry ? TEXT("true") : TEXT("false"), HasAuthority() ? TEXT("true") : TEXT("false"));

	ExecuteGeneration(Seed, bIsRetry);
	OnGenerationStarted.Broadcast(Seed, bIsRetry);
}

void AWFCNetworkActor::MulticastGenerationCompleted_Implementation(bool bSuccess, int32 UsedSeed, int32 RetryCount)
{
	UE_LOG(LogWFC, Log, TEXT("Network generation completed: Success=%s, Seed=%d, Retries=%d"), 
		bSuccess ? TEXT("true") : TEXT("false"), UsedSeed, RetryCount);

	OnGenerationCompleted.Broadcast(bSuccess, UsedSeed, RetryCount);
}

void AWFCNetworkActor::ExecuteGeneration(int32 Seed, bool bIsRetry)
{
	UWFCGeneratorComponent* WFCComp = GetWFCGenerator();
	if (!WFCComp)
	{
		UE_LOG(LogWFC, Error, TEXT("WFCGenerator component is null"));
		return;
	}

	// Configure the generator with the network seed
	WFCComp->bUseMultiplayerSeeds = true;
	WFCComp->SetSeedAndRun(Seed);
}

void AWFCNetworkActor::OnGeneratorFinished(EWFCGeneratorState State)
{
	const bool bSuccess = (State == EWFCGeneratorState::Finished);
	
	if (!bSuccess && CurrentRetryCount < MaxGenerationRetries && HasAuthority())
	{
		// Retry with a new seed
		CurrentRetryCount++;
		const int32 RetrySeed = GenerateRetrySeed(MasterSeed, CurrentRetryCount);
		
		UE_LOG(LogWFC, Warning, TEXT("Generation failed, retrying with new seed: %d (Retry %d/%d)"), 
			RetrySeed, CurrentRetryCount, MaxGenerationRetries);

		// Broadcast retry to all clients
		MulticastStartGeneration(RetrySeed, true);
		return;
	}

	// Final result - success or max retries reached
	if (HasAuthority())
	{
		MulticastGenerationCompleted(bSuccess, GetCurrentSeed(), CurrentRetryCount);
	}

	// Call parent implementation for tile spawning
	Super::OnGeneratorFinished(State);
}

int32 AWFCNetworkActor::GenerateRetrySeed(int32 BaseSeed, int32 RetryIndex)
{
	// Generate a deterministic but different seed for retry
	FRandomStream RetryRandom(BaseSeed);
	
	// Advance the stream based on retry index to get different seeds
	for (int32 i = 0; i < RetryIndex; ++i)
	{
		RetryRandom.RandHelper(1000);
	}
	
	int32 NewSeed = RetryRandom.RandHelper(INT_MAX);
	if (NewSeed == 0) NewSeed = RetryIndex + 1;
	
	return NewSeed;
}

void AWFCNetworkActor::OnRep_MasterSeed()
{
	UE_LOG(LogWFC, Log, TEXT("MasterSeed replicated: %d"), MasterSeed);
	OnSeedSynchronized.Broadcast(MasterSeed);
}

UWFCGeneratorComponent* AWFCNetworkActor::GetWFCGenerator() const
{
	return FindComponentByClass<UWFCGeneratorComponent>();
}

FString AWFCNetworkActor::GetNetworkStatusString() const
{
	TArray<FString> StatusParts;
	
	StatusParts.Add(FString::Printf(TEXT("Authority: %s"), HasAuthority() ? TEXT("true") : TEXT("false")));
	StatusParts.Add(FString::Printf(TEXT("MasterSeed: %d"), MasterSeed));
	StatusParts.Add(FString::Printf(TEXT("CurrentSeed: %d"), GetCurrentSeed()));
	StatusParts.Add(FString::Printf(TEXT("RetryCount: %d/%d"), CurrentRetryCount, MaxGenerationRetries));
	
	UWFCGeneratorComponent* WFCComp = GetWFCGenerator();
	if (WFCComp)
	{
		StatusParts.Add(FString::Printf(TEXT("GeneratorState: %d"), (int32)WFCComp->GetState()));
		StatusParts.Add(FString::Printf(TEXT("UseMultiplayerSeeds: %s"), WFCComp->bUseMultiplayerSeeds ? TEXT("true") : TEXT("false")));
	}
	
	return FString::Join(StatusParts, TEXT(" | "));
}