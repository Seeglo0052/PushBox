// Copyright Epic Games, Inc. All Rights Reserved.

#include "PushBoxCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Engine/LocalPlayer.h"
#include "Components/PushBoxInteractionComponent.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

APushBoxCharacter::APushBoxCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 1200.0f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->SetRelativeRotation(FRotator(-50.0f, 0.0f, 0.0f));
	CameraBoom->bDoCollisionTest = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void APushBoxCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

void APushBoxCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (JumpAction)
		{
			EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
			EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		}
		if (MoveAction)
		{
			EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APushBoxCharacter::Move);
		}
		if (LookAction)
		{
			EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &APushBoxCharacter::Look);
		}
		if (InteractAction)
		{
			EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &APushBoxCharacter::TryInteract);
		}
	}
}

void APushBoxCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();
	if (Controller)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void APushBoxCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	if (Controller)
	{
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void APushBoxCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	PerformInteractionCheck();
}

EPushBoxDirection APushBoxCharacter::NormalToPushDirection(const FVector& ImpactNormal) const
{
	// Project to horizontal plane and normalize ¡ª ignore any Z component.
	// This is critical: when the player is elevated (e.g., standing on a
	// OneWayDoor ramp), the ray may hit the top of the box, giving a nearly
	// vertical ImpactNormal. Projecting to 2D avoids garbage directions.
	const FVector N2D = FVector(ImpactNormal.X, ImpactNormal.Y, 0.f).GetSafeNormal();

	// If the horizontal component is too small (hit was almost purely on top),
	// this will be a zero vector. Handled by the caller using fallback logic.
	if (N2D.IsNearlyZero())
	{
		return EPushBoxDirection::None;
	}

	const float DotX = FVector::DotProduct(N2D, FVector::ForwardVector);  // +X
	const float DotY = FVector::DotProduct(N2D, FVector::RightVector);    // +Y

	if (FMath::Abs(DotX) > FMath::Abs(DotY))
	{
		return (DotX > 0) ? EPushBoxDirection::West : EPushBoxDirection::East;
	}
	else
	{
		return (DotY > 0) ? EPushBoxDirection::South : EPushBoxDirection::North;
	}
}

void APushBoxCharacter::PerformInteractionCheck()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	// Get camera view direction (where the crosshair points)
	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);

	// Ray from the player's position towards where the camera is aiming.
	// This way InteractionDistance is measured from the character, not the camera.
	const FVector TraceStart = GetActorLocation();
	const FVector TraceDir = CamRot.Vector();
	const FVector TraceEnd = TraceStart + TraceDir * InteractionCheckDistance;

	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult, TraceStart, TraceEnd, ECC_Visibility, Params);

	if (bHit && HitResult.GetActor())
	{
		UPushBoxInteractionComponent* HitInteractable =
			HitResult.GetActor()->FindComponentByClass<UPushBoxInteractionComponent>();

		if (HitInteractable)
		{
			// Distance from PLAYER to hit point
			float Distance = (TraceStart - HitResult.ImpactPoint).Size();

			if (Distance <= HitInteractable->InteractionDistance)
			{
				// Determine push direction.
				// Primary: use the impact normal (which box face was hit).
				EPushBoxDirection HitFace = NormalToPushDirection(HitResult.ImpactNormal);

				// Fallback: if the ray hit the top of the box (e.g., player on
				// an elevated OneWayDoor), the normal is nearly vertical and
				// NormalToPushDirection returns None. In that case, compute the
				// direction from the player's XY position to the box's XY position.
				if (HitFace == EPushBoxDirection::None)
				{
					FVector PlayerToBox = HitResult.GetActor()->GetActorLocation() - TraceStart;
					PlayerToBox.Z = 0.f;
					// Negate: push direction = away from the player
					HitFace = NormalToPushDirection(-PlayerToBox);

					// Ultimate fallback ¡ª should never happen
					if (HitFace == EPushBoxDirection::None)
						HitFace = EPushBoxDirection::North;
				}

				if (FocusedInteractable.Get() && FocusedInteractable.Get() != HitInteractable)
				{
					FocusedInteractable->EndFocus(this);
				}

				FocusedInteractable = HitInteractable;
				HitInteractable->BeginFocus(this, HitFace);
				return;
			}
		}
	}

	if (UPushBoxInteractionComponent* Old = FocusedInteractable.Get())
	{
		Old->EndFocus(this);
	}
	FocusedInteractable = nullptr;
}

void APushBoxCharacter::TryInteract()
{
	if (UPushBoxInteractionComponent* Interactable = FocusedInteractable.Get())
	{
		Interactable->Interact(this);
	}
}
