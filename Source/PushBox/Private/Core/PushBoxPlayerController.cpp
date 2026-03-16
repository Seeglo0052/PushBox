// Copyright PushBox Game. All Rights Reserved.

#include "Core/PushBoxPlayerController.h"
#include "Grid/PushBoxGridManager.h"
#include "Actors/PushBoxLevelActor.h"
#include "Kismet/GameplayStatics.h"

APushBoxPlayerController::APushBoxPlayerController()
{
}

void APushBoxPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Ensure mouse is captured for gameplay (camera rotation)
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	SetShowMouseCursor(false);

	FindGridManager();
}

void APushBoxPlayerController::FindGridManager()
{
	TArray<AActor*> LevelActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APushBoxLevelActor::StaticClass(), LevelActors);
	if (LevelActors.Num() > 0)
	{
		APushBoxLevelActor* LevelActor = Cast<APushBoxLevelActor>(LevelActors[0]);
		if (LevelActor)
		{
			GridManager = LevelActor->GridManager;
		}
	}
}
