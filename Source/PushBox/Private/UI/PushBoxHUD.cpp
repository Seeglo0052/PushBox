// Copyright PushBox Game. All Rights Reserved.

#include "UI/PushBoxHUD.h"
#include "UI/PushBoxPrimaryLayout.h"

void APushBoxHUD::BeginPlay()
{
	Super::BeginPlay();

	if (PrimaryLayoutClass)
	{
		APlayerController* PC = GetOwningPlayerController();
		if (PC)
		{
			PrimaryLayout = CreateWidget<UPushBoxPrimaryLayout>(PC, PrimaryLayoutClass);
			if (PrimaryLayout)
			{
				PrimaryLayout->AddToViewport();
			}
		}
	}
}
