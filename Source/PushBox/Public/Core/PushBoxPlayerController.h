// Copyright PushBox Game. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PushBoxPlayerController.generated.h"

class UPushBoxGridManager;

/**
 * Player controller for the push-box game.
 * 
 * This is a minimal controller. The character handles its own input
 * (WASD movement, mouse look, LMB interact). The controller just provides
 * a reference to the grid manager for UI/HUD purposes.
 */
UCLASS(Blueprintable)
class PUSHBOX_API APushBoxPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	APushBoxPlayerController();

	/** Reference to the grid manager for HUD/UI access */
	UPROPERTY(BlueprintReadOnly, Category = "PushBox")
	TObjectPtr<UPushBoxGridManager> GridManager;

	/** Get the grid manager. Returns null if not found. */
	UFUNCTION(BlueprintPure, Category = "PushBox")
	UPushBoxGridManager* GetGridManager() const { return GridManager; }

	UFUNCTION(BlueprintCallable, Category = "PushBox")
	void FindGridManager();

protected:
	virtual void BeginPlay() override;
};
