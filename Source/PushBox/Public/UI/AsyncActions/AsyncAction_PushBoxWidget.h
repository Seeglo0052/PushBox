// Copyright PushBox Game. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "GameplayTagContainer.h"
#include "AsyncAction_PushBoxWidget.generated.h"

class UPushBoxWidgetBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPushBoxWidgetDelegate, UPushBoxWidgetBase*, PushedWidget);

/**
 * Async action to push a soft-referenced widget onto a named widget stack.
 * Same pattern as Ele's AsyncAction_PushSoftWidget.
 */
UCLASS()
class PUSHBOX_API UAsyncAction_PushBoxWidget : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (
		WorldContext = "WorldContextObject",
		HidePin = "WorldContextObject",
		BlueprintInternalUseOnly = "true",
		DisplayName = "Push Widget To Stack"))
	static UAsyncAction_PushBoxWidget* PushWidgetToStack(
		const UObject* WorldContextObject,
		APlayerController* OwningPlayerController,
		TSoftClassPtr<UPushBoxWidgetBase> InSoftWidgetClass,
		UPARAM(meta = (Categories = "Frontend.WidgetStack")) FGameplayTag InWidgetStackTag,
		bool bFocusOnNewWidget = true);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FOnPushBoxWidgetDelegate OnWidgetCreated;

	UPROPERTY(BlueprintAssignable)
	FOnPushBoxWidgetDelegate AfterPush;

private:
	TWeakObjectPtr<UWorld> CachedWorld;
	TWeakObjectPtr<APlayerController> CachedPC;
	TSoftClassPtr<UPushBoxWidgetBase> CachedWidgetClass;
	FGameplayTag CachedStackTag;
	bool bCachedFocus = false;
};
