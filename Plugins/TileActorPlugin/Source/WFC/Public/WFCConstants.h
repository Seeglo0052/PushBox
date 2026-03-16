// Copyright Bohdon Sayre. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Central location for WFC plugin constants.
 */
namespace WFCConstants
{
	// ================================================================
	// Multiplayer Constants
	// ================================================================
	
	/** Default delay before server starts generation (allows clients to connect) */
	constexpr float MULTIPLAYER_CONNECTION_DELAY = 0.1f;
	
	/** Server generation delay for WFCTestingActor (allows clients to initialize) */
	constexpr float DEFAULT_SERVER_GENERATION_DELAY = 1.0f;
	
	// ================================================================
	// Network/RPC Constants
	// ================================================================
	
	/** Approximate maximum RPC payload size (bytes) - stay well below 65KB limit */
	constexpr int32 MAX_SAFE_RPC_PAYLOAD_SIZE = 50000;
	
	/** Size of FWFCFixedTileData structure (for bandwidth estimation) */
	constexpr int32 SIZE_OF_FIXED_TILE_DATA = 8; // 2x int32
	
	/** Maximum recommended FixedTiles to sync in single RPC */
	constexpr int32 MAX_RECOMMENDED_FIXED_TILES_PER_RPC = MAX_SAFE_RPC_PAYLOAD_SIZE / SIZE_OF_FIXED_TILE_DATA;
	
	// ================================================================
	// Validation Constants
	// ================================================================
	
	/** Default timeout for generation completion (milliseconds) */
	constexpr int32 DEFAULT_GENERATION_TIMEOUT_MS = 10000;
	
	/** Sleep interval when waiting for generation (milliseconds) */
	constexpr int32 GENERATION_WAIT_SLEEP_MS = 10;
}
