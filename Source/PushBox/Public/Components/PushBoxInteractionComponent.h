// Copyright PushBox Game. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "PushBoxTypes.h"
#include "PushBoxInteractionComponent.generated.h"

class APushBoxCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBoxBeginFocus, EPushBoxDirection, Face);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBoxEndFocus);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBoxInteract, EPushBoxDirection, PushDirection);

/**
 * Interaction component for pushable boxes.
 * Modeled after Ele project's SIInteractionComponent.
 *
 * Attach this to an APushBoxBox. When the player's crosshair ray hits
 * the owning actor, this component:
 *   - Determines which face was hit (from ImpactNormal)
 *   - Enables CustomDepth on the owner's mesh for highlight
 *   - Shows an interaction widget (optional)
 *   - On interact (LMB), broadcasts the push direction
 */
UCLASS(ClassGroup = (PushBox), meta = (BlueprintSpawnableComponent))
class PUSHBOX_API UPushBoxInteractionComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UPushBoxInteractionComponent();

	/** Max distance the player can interact from */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	float InteractionDistance = 300.f;

	/** CustomDepth stencil value for highlight */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	int32 HighlightStencilValue = 252;

	// --- Delegates ---

	UPROPERTY(BlueprintAssignable)
	FOnBoxBeginFocus OnBeginFocus;

	UPROPERTY(BlueprintAssignable)
	FOnBoxEndFocus OnEndFocus;

	UPROPERTY(BlueprintAssignable)
	FOnBoxInteract OnInteract;

	// --- Called by the character's interaction check ---

	void BeginFocus(APushBoxCharacter* Character, EPushBoxDirection HitFace);
	void EndFocus(APushBoxCharacter* Character);
	void Interact(APushBoxCharacter* Character);

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsFocused() const { return bIsFocused; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	EPushBoxDirection GetFocusedFace() const { return FocusedFace; }

protected:
	virtual void BeginPlay() override;

private:
	void SetHighlight(bool bEnable);

	bool bIsFocused = false;
	EPushBoxDirection FocusedFace = EPushBoxDirection::None;
};
