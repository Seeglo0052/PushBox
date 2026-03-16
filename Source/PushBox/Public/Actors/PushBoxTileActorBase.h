// Copyright PushBox Game. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WFCTileActorBase.h"
#include "PushBoxTypes.h"
#include "PushBoxTileActorBase.generated.h"

class UPushBoxGridManager;

/**
 * Base class for PushBox tile actors spawned by the tile system.
 *
 * Adds push-box-specific functionality on top of AWFCTileActorBase:
 * - Grid coordinate and cell data storage
 * - Direction-based mesh rotation (for OneWayDoor tiles, the mesh child
 *   is rotated so its +Y forward faces the allowed entry direction)
 * - Blueprint events for box enter/exit (so you can play animations
 *   like the one-way-door flap being pressed down)
 *
 * USAGE:
 *   Create a Blueprint inheriting from this class.
 *   Add your StaticMesh as a child of TileMeshRoot.
 *   The mesh pivot should face +Y (the "open" / enterable side).
 *   At runtime, TileMeshRoot is rotated to match OneWayDirection.
 */
UCLASS(BlueprintType, Blueprintable)
class PUSHBOX_API APushBoxTileActorBase : public AWFCTileActorBase
{
	GENERATED_BODY()

public:
	APushBoxTileActorBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Scene component that holds all visual mesh children.
	 * This component is rotated to match the OneWayDirection so that
	 * the mesh's +Y forward always points toward the allowed entry side.
	 * The actor root itself stays axis-aligned for grid logic.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PushBox|Components")
	TObjectPtr<USceneComponent> TileMeshRoot;

	// ------------------------------------------------------------------
	// Grid Data (set by APushBoxLevelActor after tile actors are spawned)
	// ------------------------------------------------------------------

	/** Grid coordinate of this tile */
	UPROPERTY(BlueprintReadOnly, Category = "PushBox|Grid")
	FIntPoint GridCoord;

	/** The full cell data from the GridManager */
	UPROPERTY(BlueprintReadOnly, Category = "PushBox|Grid")
	FPushBoxCellData CellData;

	/** Cached reference to the grid manager */
	UPROPERTY(BlueprintReadOnly, Category = "PushBox|Grid")
	TObjectPtr<UPushBoxGridManager> GridManager;

	// ------------------------------------------------------------------
	// Configuration
	// ------------------------------------------------------------------

	/**
	 * Initialize this tile with puzzle grid data.
	 * Called by APushBoxLevelActor after tile actors are spawned.
	 * Sets GridCoord, CellData, rotates mesh for OneWayDoor, etc.
	 */
	UFUNCTION(BlueprintCallable, Category = "PushBox")
	void InitializeTileData(UPushBoxGridManager* InGridManager, FIntPoint InGridCoord, const FPushBoxCellData& InCellData);

	/**
	 * Whether a box is currently on this tile.
	 * Automatically updated by NotifyBoxEntered / NotifyBoxExited.
	 * Blueprint can read this to decide initial visual state.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "PushBox|State")
	bool bHasBoxOnTile = false;

	// ------------------------------------------------------------------
	// Events бк override in Blueprint to add animations / VFX
	// ------------------------------------------------------------------

	/**
	 * Called when a box enters this tile cell.
	 * @param FromDirection  The direction the box was moving when it entered.
	 *                       Will be None if this is an initial-state notification (box starts here).
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "PushBox|Events")
	void OnBoxEntered(EPushBoxDirection FromDirection);

	/**
	 * Called when a box leaves this tile cell.
	 * @param ToDirection  The direction the box is moving as it leaves.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "PushBox|Events")
	void OnBoxExited(EPushBoxDirection ToDirection);

	/**
	 * C++ callable versions that dispatch to the BP event.
	 * APushBoxLevelActor calls these.
	 */
	void NotifyBoxEntered(EPushBoxDirection FromDirection);
	void NotifyBoxExited(EPushBoxDirection ToDirection);

protected:
	/** Apply rotation to TileMeshRoot based on OneWayDirection */
	void ApplyDirectionRotation();

	/**
	 * Convert an EPushBoxDirection to a Yaw rotation in degrees.
	 * The convention: the mesh's +Y forward (default) = North.
	 *   North (+Y) = 0бу
	 *   East  (+X) = -90бу  (i.e. 270бу)
	 *   South (-Y) = 180бу
	 *   West  (-X) = 90бу
	 */
	static float DirectionToYaw(EPushBoxDirection Dir);
};
