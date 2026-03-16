// Copyright PushBox Game. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PushBoxTypes.h"
#include "PushBoxBox.generated.h"

class UPushBoxGridManager;
class UStaticMeshComponent;
class UBoxComponent;
class UPushBoxInteractionComponent;

/**
 * A pushable box in the push-box puzzle.
 *
 * Has a PushBoxInteractionComponent for crosshair detection + highlight.
 * When the player presses LMB while focusing a face, the box is pushed
 * in the opposite direction.
 */
UCLASS(BlueprintType, Blueprintable)
class PUSHBOX_API APushBoxBox : public AActor
{
	GENERATED_BODY()

public:
	APushBoxBox();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PushBox")
	UStaticMeshComponent* BoxMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PushBox")
	UBoxComponent* BoxCollision;

	/** Interaction component ¡ª handles focus/highlight/widget */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PushBox")
	UPushBoxInteractionComponent* InteractionComp;

	UPROPERTY(BlueprintReadWrite, Category = "PushBox")
	TObjectPtr<UPushBoxGridManager> GridManager;

	UPROPERTY(BlueprintReadWrite, Category = "PushBox")
	int32 BoxIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PushBox|Movement")
	float MoveDuration = 0.2f;

	UFUNCTION(BlueprintCallable, Category = "PushBox")
	void OnPushed(EPushBoxDirection PushDir);

	UFUNCTION(BlueprintCallable, Category = "PushBox")
	void SyncToGridPosition();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Called by InteractionComp delegate when player presses LMB */
	UFUNCTION()
	void HandleInteract(EPushBoxDirection PushDirection);

private:
	bool bIsMoving = false;
	FVector MoveStartPos;
	FVector MoveEndPos;
	float MoveElapsed = 0.f;
};
