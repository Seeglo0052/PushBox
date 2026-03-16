// Copyright PushBox Game. All Rights Reserved.

#include "Actors/PushBoxTileActorBase.h"
#include "Grid/PushBoxGridManager.h"
#include "Components/SceneComponent.h"

APushBoxTileActorBase::APushBoxTileActorBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Create a scene component as mesh root, attached under the actor root.
	// Blueprint users will add their static meshes as children of TileMeshRoot.
	// We rotate TileMeshRoot to match the one-way direction; the actor root stays grid-aligned.
	TileMeshRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TileMeshRoot"));
	TileMeshRoot->SetupAttachment(RootComponent);
}

void APushBoxTileActorBase::InitializeTileData(
	UPushBoxGridManager* InGridManager,
	FIntPoint InGridCoord,
	const FPushBoxCellData& InCellData)
{
	GridManager = InGridManager;
	GridCoord = InGridCoord;
	CellData = InCellData;

	ApplyDirectionRotation();
}

void APushBoxTileActorBase::ApplyDirectionRotation()
{
	if (!TileMeshRoot) return;

	// Only rotate for tiles that have a meaningful direction
	if (CellData.TileType == EPushBoxTileType::OneWayDoor &&
		CellData.OneWayDirection != EPushBoxDirection::None)
	{
		const float Yaw = DirectionToYaw(CellData.OneWayDirection);
		TileMeshRoot->SetRelativeRotation(FRotator(0.0f, Yaw, 0.0f));

		UE_LOG(LogPushBox, Log, TEXT("TileActor [%d,%d] OneWayDoor: direction=%d, mesh yaw=%.1f"),
			GridCoord.X, GridCoord.Y, (int32)CellData.OneWayDirection, Yaw);
	}
}

float APushBoxTileActorBase::DirectionToYaw(EPushBoxDirection Dir)
{
	// Mesh default: entrance faces +Y (Yaw 0¡ã).
	// East=90¡ã and West=-90¡ã are confirmed correct.
	// N/S door buttons store the opposite direction (entry¡úmovement),
	// so the Yaw mapping matches: stored North ¡ú Yaw 180¡ã (visual south entry).
	switch (Dir)
	{
	case EPushBoxDirection::North: return 180.0f;
	case EPushBoxDirection::East:  return 90.0f;
	case EPushBoxDirection::South: return 0.0f;
	case EPushBoxDirection::West:  return -90.0f;
	default:                       return 0.0f;
	}
}

void APushBoxTileActorBase::NotifyBoxEntered(EPushBoxDirection FromDirection)
{
	bHasBoxOnTile = true;
	OnBoxEntered(FromDirection);
}

void APushBoxTileActorBase::NotifyBoxExited(EPushBoxDirection ToDirection)
{
	bHasBoxOnTile = false;
	OnBoxExited(ToDirection);
}
