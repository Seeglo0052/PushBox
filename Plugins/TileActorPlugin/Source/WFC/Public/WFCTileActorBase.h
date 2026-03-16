// Copyright Bohdon Sayre. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WFCTileActorBase.generated.h"

/**
 * Base class for WFC-generated tile actors.
 * Implements proper Net Startup Actor behavior for multiplayer procedural generation.
 * 
 * Based on Alvaro Jover-Alvarez's multiplayer procedural generation method:
 * - Deterministic naming
 * - bNetStartup = true
 * 
 * MINIMAL CONFIGURATION:
 * - Suitable for static obstacle tiles that don't change after spawn
 * - No properties need replication
 * - CharacterMovementComponent will trust these actors
 * 
 * ADVANCED CONFIGURATION:
 * - Override IsNetRelevantFor() if you need deferred replication
 * - Add replicated properties if tiles have dynamic state
 */
UCLASS(Abstract, Blueprintable)
class WFC_API AWFCTileActorBase : public AActor
{
	GENERATED_BODY()

public:
	AWFCTileActorBase(const FObjectInitializer& ObjectInitializer);

	/** 
	 * Whether this is a procedurally generated actor.
	 * Set to true automatically by WFCTestingActor.
	 * 
	 * NOTE: This property does NOT need replication for static tiles.
	 * It's only for local tracking/debugging.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "WFC")
	bool bIsProcedurallyGenerated;

	/** 
	 * Cell index in the WFC grid (for debugging and validation).
	 * 
	 * NOTE: This property does NOT need replication for static tiles.
	 * It's only for local tracking/debugging.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "WFC")
	int32 WFCCellIndex;

	// ============================================================================
	// OPTIONAL: Override IsNetRelevantFor if you need deferred replication
	// ============================================================================
	// For static obstacle tiles, this is NOT required.
	// Uncomment only if your tiles have dynamic state that needs replication.
	//
	// virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;

protected:
	// ============================================================================
	// OPTIONAL: Add this function only if you override IsNetRelevantFor
	// ============================================================================
	// virtual bool HasPlayerFinishedProcGen(const APlayerController* PC) const;
};
