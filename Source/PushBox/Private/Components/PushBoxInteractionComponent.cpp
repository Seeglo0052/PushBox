// Copyright PushBox Game. All Rights Reserved.

#include "Components/PushBoxInteractionComponent.h"
#include "Components/PrimitiveComponent.h"

UPushBoxInteractionComponent::UPushBoxInteractionComponent()
{
	// Widget is hidden by default, shown on focus
	SetHiddenInGame(true);
	SetWidgetSpace(EWidgetSpace::Screen);
	SetDrawAtDesiredSize(true);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void UPushBoxInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UPushBoxInteractionComponent::BeginFocus(APushBoxCharacter* Character, EPushBoxDirection HitFace)
{
	if (bIsFocused && FocusedFace == HitFace)
		return;

	bIsFocused = true;
	FocusedFace = HitFace;

	SetHighlight(true);

	// Show the interaction widget
	SetHiddenInGame(false);

	OnBeginFocus.Broadcast(HitFace);
}

void UPushBoxInteractionComponent::EndFocus(APushBoxCharacter* Character)
{
	if (!bIsFocused)
		return;

	bIsFocused = false;
	FocusedFace = EPushBoxDirection::None;

	SetHighlight(false);

	// Hide the interaction widget
	SetHiddenInGame(true);

	OnEndFocus.Broadcast();
}

void UPushBoxInteractionComponent::Interact(APushBoxCharacter* Character)
{
	if (!bIsFocused)
		return;

	OnInteract.Broadcast(FocusedFace);
}

void UPushBoxInteractionComponent::SetHighlight(bool bEnable)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	// Exactly the same approach as Ele's SIInteractionComponent:
	// Set CustomDepth on ALL primitive components of the owner.
	TArray<UActorComponent*> FoundComponents;
	Owner->GetComponents(UPrimitiveComponent::StaticClass(), FoundComponents);

	for (auto& FoundComponent : FoundComponents)
	{
		if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(FoundComponent))
		{
			if (Prim == this) continue; // Skip the widget component itself

			if (bEnable)
			{
				Prim->SetCustomDepthStencilValue(HighlightStencilValue);
				Prim->SetRenderCustomDepth(true);
			}
			else
			{
				Prim->SetRenderCustomDepth(false);
			}
		}
	}
}
