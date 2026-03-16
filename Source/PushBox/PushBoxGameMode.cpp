// Copyright PushBox Game. All Rights Reserved.

#include "PushBoxGameMode.h"
#include "Core/PushBoxPlayerController.h"
#include "UI/PushBoxHUD.h"
#include "PushBoxCharacter.h"
#include "UObject/ConstructorHelpers.h"

APushBoxGameMode::APushBoxGameMode()
{
	// Default controller ¡ª override with BP_PushBoxPlayerController in your GameMode blueprint
	PlayerControllerClass = APushBoxPlayerController::StaticClass();
	HUDClass = APushBoxHUD::StaticClass();

	// Default pawn ¡ª override with your character BP in GameMode blueprint
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
