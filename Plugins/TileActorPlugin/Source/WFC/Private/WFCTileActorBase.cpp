// Copyright Bohdon Sayre. All Rights Reserved.

#include "WFCTileActorBase.h"
#include "WFCModule.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

AWFCTileActorBase::AWFCTileActorBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bIsProcedurallyGenerated(false)
	, WFCCellIndex(INDEX_NONE)
{
	// ============================================================================
	// MINIMAL CONFIGURATION FOR STATIC OBSTACLE TILES
	// ============================================================================
	
	// Enable replication (Actor exists on both server and client)
	bReplicates = true;
	
	// Use default relevance check (good for static tiles)
	bAlwaysRelevant = false;
	
	// ============================================================================
	// OPTIMIZATION FOR STATIC TILES
	// ============================================================================
	
	// Static tiles don't need frequent updates
	NetUpdateFrequency = 1.0f;  // 1 Hz (very low frequency)
	
	// Distance culling (only sync nearby tiles)
	NetCullDistanceSquared = 15000.0f * 15000.0f;  // 150 meters
	
	// Medium priority (not critical for gameplay)
	NetPriority = 2.0f;
	
	// ============================================================================
	// NOTES:
	// ============================================================================
	// - bNetStartup will be set to true in SpawnActorForCell()
	// - Deterministic naming is handled by SpawnActorForCell()
	// - No properties need replication for static tiles
	// - CharacterMovementComponent will trust these actors via NetGUID
}

// ============================================================================
// OPTIONAL: Uncomment this section if you need deferred replication
// ============================================================================/

/*
bool AWFCTileActorBase::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
	// For static obstacle tiles, you typically don't need this.
	// Only use if your tiles have dynamic state that needs deferred replication.
	
	if (bIsProcedurallyGenerated)
	{
		if (const APlayerController* PC = Cast<APlayerController>(RealViewer))
		{
			if (!HasPlayerFinishedProcGen(PC))
			{
				return false;  // Defer replication until client finishes generation
			}
		}
	}

	return Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
}

bool AWFCTileActorBase::HasPlayerFinishedProcGen(const APlayerController* PC) const
{
	if (!PC)
	{
		return false;
	}

	// Option 1: Check custom flag on PlayerController
	// if (const AYourPlayerController* CustomPC = Cast<AYourPlayerController>(PC))
	// {
	//     return CustomPC->bClientFinishedProceduralGeneration;
	// }

	// Option 2: Simple time-based approach
	const float TimeSinceConnection = GetWorld()->GetTimeSeconds() - PC->GetWorld()->GetTimeSeconds();
	const float GenerationTimeout = 5.0f;
	
	return TimeSinceConnection > GenerationTimeout;
}
*/
