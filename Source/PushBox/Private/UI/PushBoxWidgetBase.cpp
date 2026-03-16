// Copyright PushBox Game. All Rights Reserved.

#include "UI/PushBoxWidgetBase.h"
#include "Core/PushBoxPlayerController.h"
#include "Grid/PushBoxGridManager.h"

APushBoxPlayerController* UPushBoxWidgetBase::GetPushBoxPlayerController()
{
	if (!CachedPC.IsValid())
	{
		CachedPC = Cast<APushBoxPlayerController>(GetOwningPlayer());
	}
	return CachedPC.Get();
}

UPushBoxGridManager* UPushBoxWidgetBase::GetGridManager()
{
	APushBoxPlayerController* PC = GetPushBoxPlayerController();
	return PC ? PC->GetGridManager() : nullptr;
}
