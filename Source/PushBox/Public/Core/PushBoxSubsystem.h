// Copyright PushBox Game. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PushBoxTypes.h"
#include "PushBoxSubsystem.generated.h"

class UPushBoxGridManager;

/**
 * World subsystem that provides a central access point to the active puzzle.
 * Any Blueprint or C++ code can get this subsystem to query puzzle state.
 */
UCLASS()
class PUSHBOX_API UPushBoxSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Register the active grid manager (called by PushBoxLevelActor) */
	UFUNCTION(BlueprintCallable, Category = "PushBox")
	void RegisterGridManager(UPushBoxGridManager* InManager);

	/** Get the active grid manager */
	UFUNCTION(BlueprintPure, Category = "PushBox")
	UPushBoxGridManager* GetGridManager() const { return ActiveGridManager; }

	/** Get current puzzle state */
	UFUNCTION(BlueprintPure, Category = "PushBox")
	EPuzzleState GetPuzzleState() const;

	/** Get current move count */
	UFUNCTION(BlueprintPure, Category = "PushBox")
	int32 GetMoveCount() const;

private:
	UPROPERTY()
	TObjectPtr<UPushBoxGridManager> ActiveGridManager;
};
