// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "PushBoxTypes.h"
#include "PushBoxCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UPushBoxInteractionComponent;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 * Standard TPS character for the push-box game.
 * 
 * WASD = movement, Mouse = camera, LMB = interact (push box).
 * The character itself knows nothing about the puzzle grid.
 * Crosshair ray trace detects boxes and highlights their faces.
 * LMB pushes the currently focused box.
 */
UCLASS(config=Game)
class APushBoxCharacter : public ACharacter
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* InteractAction;

public:
	APushBoxCharacter();

	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float InteractionCheckDistance = 2000.f;

	UFUNCTION(BlueprintPure, Category = "Interaction")
	UPushBoxInteractionComponent* GetFocusedInteractable() const { return FocusedInteractable.Get(); }

protected:
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Tick(float DeltaTime) override;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void TryInteract();
	void PerformInteractionCheck();

	EPushBoxDirection NormalToPushDirection(const FVector& ImpactNormal) const;

public:
	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }

private:
	/** The interaction component we're currently looking at */
	TWeakObjectPtr<UPushBoxInteractionComponent> FocusedInteractable;
};

