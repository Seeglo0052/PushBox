// Copyright PushBox Game. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PushBoxTypes.h"
#include "PushBoxFunctionLibrary.generated.h"

class UPushBoxGridManager;

/**
 * Static utility functions for the PushBox puzzle system.
 */
UCLASS()
class PUSHBOX_API UPushBoxFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Get the active grid manager from the world */
	UFUNCTION(BlueprintPure, Category = "PushBox", meta = (WorldContext = "WorldContextObject"))
	static UPushBoxGridManager* GetGridManager(const UObject* WorldContextObject);

	/** Get the direction as a readable string */
	UFUNCTION(BlueprintPure, Category = "PushBox")
	static FString DirectionToString(EPushBoxDirection Direction);

	/** Get the tile type as a readable string */
	UFUNCTION(BlueprintPure, Category = "PushBox")
	static FString TileTypeToString(EPushBoxTileType TileType);

	/** Reverse a move sequence (for converting reverse-edit moves to forward solution) */
	UFUNCTION(BlueprintCallable, Category = "PushBox|Editor")
	static TArray<FPushBoxMoveRecord> ReverseMoveSequence(const TArray<FPushBoxMoveRecord>& InMoves);

	// ------------------------------------------------------------------
	// Runtime Gameplay
	// ------------------------------------------------------------------

	/** Restart the current puzzle ¡ª reset all boxes and player to their start positions.
	 *  Call this from a "Restart" button in your gameplay UI. */
	UFUNCTION(BlueprintCallable, Category = "PushBox|Gameplay", meta = (WorldContext = "WorldContextObject"))
	static void RestartCurrentPuzzle(const UObject* WorldContextObject);

	/** Quit the game application. Works in packaged builds and PIE. */
	UFUNCTION(BlueprintCallable, Category = "PushBox|Gameplay", meta = (WorldContext = "WorldContextObject"))
	static void QuitGame(const UObject* WorldContextObject);

	// ------------------------------------------------------------------
	// Theme Color
	// ------------------------------------------------------------------

	/**
	 * Get all available atlas color texture paths.
	 * Returns soft object paths to T_Atlas, T_Atlas_Blue, T_Atlas_Red, etc.
	 */
	UFUNCTION(BlueprintCallable, Category = "PushBox|Theme")
	static TArray<FSoftObjectPath> GetAllAtlasColorTextures();

	/**
	 * Pick a random atlas color texture (different from the current one if possible).
	 * Returns the soft object path to the chosen texture.
	 */
	UFUNCTION(BlueprintCallable, Category = "PushBox|Theme")
	static FSoftObjectPath GetRandomAtlasColorTexture();

	/**
	 * Apply a theme color texture to the MI_Atlas material instance.
	 * Loads the texture and sets it as the "base color" parameter on MI_Atlas.
	 * Call this at level start for a fresh color each play session.
	 *
	 * @param TexturePath  Soft path to one of the T_Atlas_* textures (use GetRandomAtlasColorTexture)
	 * @return True if the material parameter was set successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "PushBox|Theme", meta = (WorldContext = "WorldContextObject"))
	static bool ApplyAtlasThemeColor(const UObject* WorldContextObject, const FSoftObjectPath& TexturePath);
};
