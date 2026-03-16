// Copyright PushBox Game. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "PushBoxWidgetBase.generated.h"

class APushBoxPlayerController;
class UPushBoxGridManager;

/**
 * Base class for all PushBox activatable widgets.
 * Uses CommonUI's activatable widget system (same pattern as Ele project).
 */
UCLASS(Abstract, BlueprintType, meta = (DisableNativeTick))
class PUSHBOX_API UPushBoxWidgetBase : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	UFUNCTION(BlueprintPure, Category = "PushBox|UI")
	APushBoxPlayerController* GetPushBoxPlayerController();

	/** Get the grid manager. Returns null if not available yet. */
	UFUNCTION(BlueprintPure, Category = "PushBox|UI")
	UPushBoxGridManager* GetGridManager();

private:
	TWeakObjectPtr<APushBoxPlayerController> CachedPC;
};
