// Copyright PushBox Game. All Rights Reserved.

#include "UI/PushBoxPrimaryLayout.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

UCommonActivatableWidgetContainerBase* UPushBoxPrimaryLayout::FindWidgetStackByTag(const FGameplayTag& InTag) const
{
	const TObjectPtr<UCommonActivatableWidgetContainerBase>* Found = RegisteredWidgetStackMap.Find(InTag);
	return Found ? Found->Get() : nullptr;
}

void UPushBoxPrimaryLayout::RegisterWidgetStack(FGameplayTag InStackTag, UCommonActivatableWidgetContainerBase* InStack)
{
	if (InStack && InStackTag.IsValid())
	{
		RegisteredWidgetStackMap.Add(InStackTag, InStack);
	}
}
