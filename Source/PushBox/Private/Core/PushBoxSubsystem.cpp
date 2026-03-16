// Copyright PushBox Game. All Rights Reserved.

#include "Core/PushBoxSubsystem.h"
#include "Grid/PushBoxGridManager.h"

void UPushBoxSubsystem::RegisterGridManager(UPushBoxGridManager* InManager)
{
	ActiveGridManager = InManager;
}

EPuzzleState UPushBoxSubsystem::GetPuzzleState() const
{
	return ActiveGridManager ? ActiveGridManager->CurrentPuzzleState : EPuzzleState::NotStarted;
}

int32 UPushBoxSubsystem::GetMoveCount() const
{
	return ActiveGridManager ? ActiveGridManager->MoveCount : 0;
}
