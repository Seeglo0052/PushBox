// Copyright PushBox Game. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "GameplayTagContainer.h"
#include "PushBoxPrimaryLayout.generated.h"

class UCommonActivatableWidgetContainerBase;

/**
 * Primary layout widget. Manages widget stacks for game HUD and menus.
 * Same pattern as Ele's Widget_PrimaryLayout.
 */
UCLASS(Abstract, BlueprintType, meta = (DisableNativeTick))
class PUSHBOX_API UPushBoxPrimaryLayout : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** Find a registered widget stack by gameplay tag */
	UCommonActivatableWidgetContainerBase* FindWidgetStackByTag(const FGameplayTag& InTag) const;

protected:
	/** Register a widget stack container with a tag (call from Blueprint) */
	UFUNCTION(BlueprintCallable, Category = "PushBox|UI")
	void RegisterWidgetStack(
		UPARAM(meta = (Categories = "Frontend.WidgetStack")) FGameplayTag InStackTag,
		UCommonActivatableWidgetContainerBase* InStack);

private:
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UCommonActivatableWidgetContainerBase>> RegisteredWidgetStackMap;
};
