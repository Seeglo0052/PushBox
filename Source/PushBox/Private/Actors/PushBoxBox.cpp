// Copyright PushBox Game. All Rights Reserved.

#include "Actors/PushBoxBox.h"
#include "Grid/PushBoxGridManager.h"
#include "Components/PushBoxInteractionComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"

APushBoxBox::APushBoxBox()
{
	PrimaryActorTick.bCanEverTick = true;

	BoxCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxCollision"));
	BoxCollision->SetBoxExtent(FVector(50.f, 50.f, 50.f));
	BoxCollision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	// Ensure Visibility trace channel responds (for crosshair ray detection)
	BoxCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	SetRootComponent(BoxCollision);

	BoxMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoxMesh"));
	BoxMesh->SetupAttachment(BoxCollision);
	// Mesh has no collision ¡ª collision comes from BoxCollision
	BoxMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	InteractionComp = CreateDefaultSubobject<UPushBoxInteractionComponent>(TEXT("InteractionComp"));
	InteractionComp->SetupAttachment(BoxCollision);
}

void APushBoxBox::BeginPlay()
{
	Super::BeginPlay();

	// Bind the interaction delegate so LMB triggers OnPushed
	if (InteractionComp)
	{
		InteractionComp->OnInteract.AddDynamic(this, &APushBoxBox::HandleInteract);
	}

	SyncToGridPosition();
}

void APushBoxBox::HandleInteract(EPushBoxDirection PushDirection)
{
	OnPushed(PushDirection);
}

void APushBoxBox::OnPushed(EPushBoxDirection PushDir)
{
	if (!GridManager || BoxIndex == INDEX_NONE || bIsMoving)
		return;

	if (!GridManager->BoxPositions.IsValidIndex(BoxIndex))
		return;

	const FIntPoint BoxCoord = GridManager->BoxPositions[BoxIndex];
	const FIntPoint Offset = UPushBoxGridManager::GetDirectionOffset(PushDir);
	const FIntPoint NewBoxCoord = BoxCoord + Offset;

	UE_LOG(LogTemp, Log, TEXT("OnPushed: Box[%d] at (%d,%d), PushDir=%d, Target=(%d,%d)"),
		BoxIndex, BoxCoord.X, BoxCoord.Y, (int32)PushDir, NewBoxCoord.X, NewBoxCoord.Y);

	// Verify grid is initialized
	if (GridManager->Cells.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("OnPushed: Grid Cells array is EMPTY! GridSize=(%d,%d)"),
			GridManager->GridSize.X, GridManager->GridSize.Y);
		return;
	}

	// Check target coord validity first
	if (!GridManager->IsValidCoord(NewBoxCoord))
	{
		UE_LOG(LogTemp, Warning, TEXT("OnPushed: Target (%d,%d) is out of bounds"), NewBoxCoord.X, NewBoxCoord.Y);
		return;
	}

	// Check if the box can enter the target cell
	if (!GridManager->CanBoxEnterCell(NewBoxCoord, PushDir))
	{
		FPushBoxCellData TargetCell = GridManager->GetCellData(NewBoxCoord);
		UE_LOG(LogTemp, Warning, TEXT("OnPushed: BLOCKED. Target cell type=%d, HasBox=%d"),
			(int32)TargetCell.TileType, GridManager->HasBoxAt(NewBoxCoord) ? 1 : 0);
		return;
	}

	// Move the box in the grid
	FIntPoint FinalPos = NewBoxCoord;

	// Ice: slide until hitting something
	FPushBoxCellData DestCell = GridManager->GetCellData(NewBoxCoord);
	if (DestCell.TileType == EPushBoxTileType::Ice)
	{
		FIntPoint SlidePos = NewBoxCoord;
		for (;;)
		{
			FIntPoint Next = SlidePos + Offset;
			if (!GridManager->IsValidCoord(Next)) break;
			FPushBoxCellData NC = GridManager->GetCellData(Next);
			if (NC.TileType == EPushBoxTileType::Wall) break;
			if (GridManager->HasBoxAt(Next)) break;
			if (NC.TileType == EPushBoxTileType::OneWayDoor && NC.OneWayDirection != PushDir) break;
			SlidePos = Next;
			FPushBoxCellData SC = GridManager->GetCellData(SlidePos);
			if (SC.TileType != EPushBoxTileType::Ice) break;
		}
		FinalPos = SlidePos;
	}

	UE_LOG(LogTemp, Log, TEXT("OnPushed: Moving box from (%d,%d) to (%d,%d)"),
		BoxCoord.X, BoxCoord.Y, FinalPos.X, FinalPos.Y);

	// Update grid state
	GridManager->BoxPositions[BoxIndex] = FinalPos;
	GridManager->OnBoxMoved.Broadcast(BoxCoord, FinalPos);

	// Record move for undo
	FPushBoxMoveRecord Rec;
	Rec.Direction = PushDir;
	Rec.PlayerFrom = FIntPoint(-1, -1);
	Rec.PlayerTo = FIntPoint(-1, -1);
	Rec.BoxFrom = BoxCoord;
	Rec.BoxTo = FinalPos;
	Rec.bIsReverse = false;
	GridManager->MoveHistory.Add(Rec);
	++GridManager->MoveCount;

	// Start visual movement
	MoveStartPos = GetActorLocation();
	MoveEndPos = GridManager->GridToWorld(FinalPos) + FVector(0, 0, 50.f);
	MoveElapsed = 0.f;
	bIsMoving = true;

	// Check win condition
	if (GridManager->CheckWinCondition())
	{
		GridManager->SetPuzzleState(EPuzzleState::Completed);
	}
}

void APushBoxBox::SyncToGridPosition()
{
	if (!GridManager || !GridManager->BoxPositions.IsValidIndex(BoxIndex))
		return;

	const FIntPoint Coord = GridManager->BoxPositions[BoxIndex];
	const FVector WorldPos = GridManager->GridToWorld(Coord) + FVector(0, 0, 50.f);
	SetActorLocation(WorldPos);
}

void APushBoxBox::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsMoving)
	{
		MoveElapsed += DeltaTime;
		float Alpha = FMath::Clamp(MoveElapsed / MoveDuration, 0.f, 1.f);
		// Smooth step for nicer feel
		Alpha = FMath::SmoothStep(0.f, 1.f, Alpha);

		SetActorLocation(FMath::Lerp(MoveStartPos, MoveEndPos, Alpha));

		if (Alpha >= 1.f)
		{
			bIsMoving = false;
			SyncToGridPosition(); // Snap exactly
		}
	}
}
