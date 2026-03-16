// Copyright PushBox Game. All Rights Reserved.

#include "UI/AsyncActions/AsyncAction_PushBoxWidget.h"
#include "UI/PushBoxWidgetBase.h"
#include "UI/PushBoxPrimaryLayout.h"
#include "UI/PushBoxHUD.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"

UAsyncAction_PushBoxWidget* UAsyncAction_PushBoxWidget::PushWidgetToStack(
	const UObject* WorldContextObject,
	APlayerController* OwningPlayerController,
	TSoftClassPtr<UPushBoxWidgetBase> InSoftWidgetClass,
	FGameplayTag InWidgetStackTag,
	bool bFocusOnNewWidget)
{
	UAsyncAction_PushBoxWidget* Action = NewObject<UAsyncAction_PushBoxWidget>();
	Action->CachedWorld = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	Action->CachedPC = OwningPlayerController;
	Action->CachedWidgetClass = InSoftWidgetClass;
	Action->CachedStackTag = InWidgetStackTag;
	Action->bCachedFocus = bFocusOnNewWidget;
	Action->RegisterWithGameInstance(WorldContextObject);
	return Action;
}

void UAsyncAction_PushBoxWidget::Activate()
{
	if (!CachedPC.IsValid() || CachedWidgetClass.IsNull())
	{
		SetReadyToDestroy();
		return;
	}

	// Load the widget class
	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
	Streamable.RequestAsyncLoad(
		CachedWidgetClass.ToSoftObjectPath(),
		FStreamableDelegate::CreateWeakLambda(this, [this]()
		{
			UClass* WidgetClass = CachedWidgetClass.Get();
			if (!WidgetClass || !CachedPC.IsValid())
			{
				SetReadyToDestroy();
				return;
			}

			// Find primary layout through HUD
			APushBoxHUD* HUD = CachedPC->GetHUD<APushBoxHUD>();
			if (!HUD || !HUD->GetPrimaryLayout())
			{
				SetReadyToDestroy();
				return;
			}

			UCommonActivatableWidgetContainerBase* Stack =
				HUD->GetPrimaryLayout()->FindWidgetStackByTag(CachedStackTag);
			if (!Stack)
			{
				SetReadyToDestroy();
				return;
			}

			UPushBoxWidgetBase* Widget = Stack->AddWidget<UPushBoxWidgetBase>(
				TSubclassOf<UCommonActivatableWidget>(WidgetClass));

			if (Widget)
			{
				OnWidgetCreated.Broadcast(Widget);
				AfterPush.Broadcast(Widget);
			}

			SetReadyToDestroy();
		})
	);
}
