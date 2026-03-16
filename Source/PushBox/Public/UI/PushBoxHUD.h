// Copyright PushBox Game. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PushBoxHUD.generated.h"

class UPushBoxPrimaryLayout;

/**
 * HUD that creates the primary layout widget on BeginPlay.
 */
UCLASS()
class PUSHBOX_API APushBoxHUD : public AHUD
{
	GENERATED_BODY()

public:
	/** The widget class for the primary layout (set in BP) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UPushBoxPrimaryLayout> PrimaryLayoutClass;

	/** The instantiated primary layout */
	UPROPERTY(BlueprintReadOnly, Category = "UI")
	TObjectPtr<UPushBoxPrimaryLayout> PrimaryLayout;

	/** Get the primary layout */
	UFUNCTION(BlueprintPure, Category = "UI")
	UPushBoxPrimaryLayout* GetPrimaryLayout() const { return PrimaryLayout; }

protected:
	virtual void BeginPlay() override;
};
