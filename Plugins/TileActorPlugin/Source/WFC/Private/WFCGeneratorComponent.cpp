// Copyright Bohdon Sayre. All Rights Reserved.


#include "WFCGeneratorComponent.h"

#include "WFCAsset.h"
#include "WFCModule.h"
#include "WFCStatics.h"
#include "WFCTestingActor.h"
#include "Core/WFCGenerator.h"
#include "Core/WFCGrid.h"
#include "Core/Constraints/WFCFixedTileConstraint.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include <Net/UnrealNetwork.h>


UWFCGeneratorComponent::UWFCGeneratorComponent()
	: StepLimit(100000),
	  bAutoRun(true),
	  bUseMultiplayerSeeds(false),
	  bRequestSeedFromServer(true),
	  RandomSeed(0),
	  bWaitingForSeed(false)
{
	// Enable replication for this component
	SetIsReplicatedByDefault(true);
}

void UWFCGeneratorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UWFCGeneratorComponent, RandomSeed);
}

void UWFCGeneratorComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoRun)
	{
		HandleAutoRun();
	}
}

void UWFCGeneratorComponent::HandleAutoRun()
{
	if (!bUseMultiplayerSeeds)
	{
		// Standard single-player generation
		Run();
		return;
	}

	// Multiplayer generation handling
	if (GetOwner()->HasAuthority())
	{
		// Server: Generate seed if needed
		if (RandomSeed == 0)
		{
			RandomSeed = GenerateNewSeed();
		}
		RandomStream.Initialize(RandomSeed);
		
		// Use the same multicast mechanism as Run() for consistency
		// This notifies all connected clients and starts generation
		MulticastStartGeneration(RandomSeed);
		
		// Server also executes generation
		StartGeneration();
		
		UE_LOG(LogWFC, Log, TEXT("Server auto-run: Broadcasting seed %d to all clients"), RandomSeed);
	}
	else
	{
		// Client: Just wait for the server's MulticastStartGeneration
		// The server will broadcast when ready via BeginPlay or Run()
		UE_LOG(LogWFC, Log, TEXT("Client auto-run: Waiting for server multicast"));
		
		// Optional: If client has a seed already (e.g., from save), use it
		if (RandomSeed != 0)
		{
			RandomStream.Initialize(RandomSeed);
			UE_LOG(LogWFC, Log, TEXT("Client has existing seed %d, waiting for server sync"), RandomSeed);
		}
	}
}

bool UWFCGeneratorComponent::Initialize(bool bForce)
{
	if (!bForce && IsInitialized())
	{
		// already initialized
		return true;
	}

	if (!WFCAsset)
	{
		UE_LOG(LogWFC, Warning, TEXT("No WFCAsset was specified: %s"), *GetNameSafe(GetOwner()));
		return false;
	}

	Generator = UWFCStatics::CreateWFCGenerator(this, WFCAsset);
	if (!Generator)
	{
		return false;
	}

	Generator->OnCellSelected.AddUObject(this, &UWFCGeneratorComponent::OnCellSelected);
	Generator->OnStateChanged.AddUObject(this, &UWFCGeneratorComponent::OnStateChanged);

	Generator->Initialize(false);
	Generator->InitializeConstraints();

	// Set the random stream for the generator
	if (bUseMultiplayerSeeds && RandomSeed != 0)
	{
		RandomStream.Initialize(RandomSeed);
		Generator->SetRandomStream(RandomStream);
	}

	return true;
}

bool UWFCGeneratorComponent::IsInitialized() const
{
	return Generator && Generator->IsInitialized();
}

void UWFCGeneratorComponent::ResetGenerator()
{
	if (!Generator)
	{
		// No generator exists, initialize from scratch
		Initialize(true);
		return;
	}

	// Use lightweight reset that preserves constraints and their dynamic state
	// This is important for iterative fixing which adds dynamic fixed tile mappings
	Generator->Reset();

	UE_LOG(LogWFC, Log, TEXT("ResetGenerator: Used lightweight reset (preserves dynamic fixed tiles)"));
}

void UWFCGeneratorComponent::Run()
{
	// 自动检测网络环境并启用多人游戏支持
	if (GetOwner() && GetWorld() && GetWorld()->GetNetMode() != NM_Standalone)
	{
		// 在多人游戏环境中自动启用多人种子同步
		if (!bUseMultiplayerSeeds)
		{
			bUseMultiplayerSeeds = true;
			bRequestSeedFromServer = !GetOwner()->HasAuthority();
			UE_LOG(LogWFC, Log, TEXT("Auto-enabled multiplayer seeds for network game"));
		}

		// 服务器端逻辑：生成种子并通知所有客户端
		if (GetOwner()->HasAuthority())
		{
			// 检查是否需要重置之前完成的生成
			if (IsInitialized() && (GetState() == EWFCGeneratorState::Finished || GetState() == EWFCGeneratorState::Error))
			{
				UE_LOG(LogWFC, Log, TEXT("Server resetting previous generation (state: %d)"), (int32)GetState());
				// 销毁之前生成的Actor
				if (GetOwner()->IsA<AWFCTestingActor>())
				{
					static_cast<AWFCTestingActor*>(GetOwner())->DestroyAllSpawnedActors();
				}
			}

			// 生成新种子（如果还没有或者需要新的种子）
			if (RandomSeed == 0 || (GetState() == EWFCGeneratorState::Finished || GetState() == EWFCGeneratorState::Error))
			{
				RandomSeed = GenerateNewSeed();
				UE_LOG(LogWFC, Log, TEXT("Server generated new seed for Run(): %d"), RandomSeed);
			}

			// 初始化随机流
			RandomStream.Initialize(RandomSeed);

			// 通过RPC通知所有客户端开始生成
			MulticastStartGeneration(RandomSeed);
			
			// 服务器也执行生成
			StartGeneration();
		}
		else
		{
			// 客户端：请求服务器启动生成
			UE_LOG(LogWFC, Log, TEXT("Client requesting server to start generation"));
			ServerRequestGeneration();
		}
	}
	else
	{
		// 单人游戏模式：直接执行原有逻辑
		if (!IsInitialized())
		{
			Initialize();
		}

		if (IsInitialized())
		{
			// 检查是否需要重置
			if (GetState() == EWFCGeneratorState::Finished || GetState() == EWFCGeneratorState::Error)
			{
				UE_LOG(LogWFC, Log, TEXT("Single-player resetting previous generation"));
				Generator->Reset();
			}
			
			Generator->Run(StepLimit);
		}
	}
}

void UWFCGeneratorComponent::SetSeedAndRun(int32 Seed)
{
	RandomSeed = Seed;
	RandomStream.Initialize(RandomSeed);
	
	if (Generator)
	{
		Generator->SetRandomStream(RandomStream);
	}
	
	StartGeneration();
}

int32 UWFCGeneratorComponent::GetRandomSeed() const
{
	return RandomSeed;
}

void UWFCGeneratorComponent::StartGeneration()
{
	if (!IsInitialized())
	{
		Initialize();
	}

	if (IsInitialized())
	{
		if (bUseMultiplayerSeeds && Generator)
		{
			Generator->SetRandomStream(RandomStream);
			// 在多人游戏中强制确定性模式
			Generator->bForceDeterministic = true;
			UE_LOG(LogWFC, Warning, TEXT("Enabled force deterministic mode for multiplayer"));
		}
		Generator->Run(StepLimit);
	}
}

const UWFCGrid* UWFCGeneratorComponent::GetGrid() const
{
	return Generator ? Generator->GetGrid() : nullptr;
}

EWFCGeneratorState UWFCGeneratorComponent::GetState() const
{
	return Generator ? Generator->State : EWFCGeneratorState::None;
}

void UWFCGeneratorComponent::GetSelectedTileId(int32 CellIndex, bool& bSuccess, int32& TileId) const
{
	TileId = INDEX_NONE;
	bSuccess = false;
	if (Generator)
	{
		const FWFCCell& Cell = Generator->GetCell(CellIndex);
		if (Cell.HasSelection())
		{
			TileId = Cell.GetSelectedTileId();
			bSuccess = true;
		}
	}
}

void UWFCGeneratorComponent::GetSelectedTileIds(TArray<int32>& TileIds) const
{
	if (Generator)
	{
		Generator->GetSelectedTileIds(TileIds);
	}
	else
	{
		TileIds.Reset();
	}
}

// Multiplayer RPC Implementations

void UWFCGeneratorComponent::ServerRequestSeed_Implementation()
{
	// Server generates a new seed if none exists
	if (RandomSeed == 0)
	{
		RandomSeed = GenerateNewSeed();
	}

	// Send seed back to the requesting client
	ClientReceiveSeed(RandomSeed);

	UE_LOG(LogWFC, Log, TEXT("Server sent seed %d to client"), RandomSeed);
}

bool UWFCGeneratorComponent::ServerRequestSeed_Validate()
{
	return true;
}

void UWFCGeneratorComponent::ClientReceiveSeed_Implementation(int32 Seed)
{
	RandomSeed = Seed;
	RandomStream.Initialize(RandomSeed);
	bWaitingForSeed = false;

	UE_LOG(LogWFC, Log, TEXT("Client received seed %d from server"), RandomSeed);

	// Broadcast the seed received event
	OnSeedReceivedEvent_BP.Broadcast(RandomSeed);

	// If we were waiting for a seed to auto-run, start generation now
	if (bAutoRun)
	{
		StartGeneration();
	}
}

void UWFCGeneratorComponent::ServerRequestGeneration_Implementation()
{
	UE_LOG(LogWFC, Log, TEXT("Client requested generation start"));
	
	// 服务器收到客户端请求后，启动生成流程
	Run();
}

bool UWFCGeneratorComponent::ServerRequestGeneration_Validate()
{
	return true;
}

void UWFCGeneratorComponent::MulticastStartGeneration_Implementation(int32 Seed)
{
	// 所有客户端（包括服务器）接收到种子并开始生成
	if (!GetOwner()->HasAuthority()) // 只有客户端执行，服务器已经在Run()中执行过了
	{
		// 检查是否需要重置之前的生成
		if (IsInitialized() && (GetState() == EWFCGeneratorState::Finished || GetState() == EWFCGeneratorState::Error))
		{
			UE_LOG(LogWFC, Log, TEXT("Client resetting previous generation"));
		}

		PrepareClientForGeneration(Seed);  // Use helper function
		
		UE_LOG(LogWFC, Warning, TEXT("CLIENT received multicast seed %d and starting generation"), RandomSeed);
		
		// 开始本地生成
		StartGeneration();
	}
	else
	{
		UE_LOG(LogWFC, Warning, TEXT("SERVER received own multicast seed %d (ignored)"), Seed);
	}
}

void UWFCGeneratorComponent::OnRep_RandomSeed()
{
	if (RandomSeed != 0)
	{
		RandomStream.Initialize(RandomSeed);
		
		if (Generator)
		{
			Generator->SetRandomStream(RandomStream);
		}

		UE_LOG(LogWFC, Log, TEXT("RandomSeed replicated: %d"), RandomSeed);
	}
}

void UWFCGeneratorComponent::MulticastSyncFixedTiles_Implementation(const TArray<FWFCFixedTileData>& FixedTiles)
{
	// Only clients execute (server already applied locally)
	if (!GetOwner() || GetOwner()->HasAuthority())
	{
		UE_LOG(LogWFC, Verbose, TEXT("Server received own MulticastSyncFixedTiles (ignored)"));
		return;
	}

	UE_LOG(LogWFC, Log, TEXT("========================================"));
	UE_LOG(LogWFC, Log, TEXT("CLIENT: Receiving %d fixed tiles from server"), FixedTiles.Num());
	UE_LOG(LogWFC, Log, TEXT("========================================"));

	if (!Generator)
	{
		UE_LOG(LogWFC, Error, TEXT("CLIENT: Cannot sync fixed tiles - Generator is null!"));
		return;
	}

	// Find the FixedTileConstraint
	UWFCFixedTileConstraint* FixedConstraint = Generator->GetConstraint<UWFCFixedTileConstraint>();
	if (!FixedConstraint)
	{
		UE_LOG(LogWFC, Error, TEXT("CLIENT: Cannot sync fixed tiles - No FixedTileConstraint found!"));
		return;
	}

	// SPECIAL CASE: Empty array means clear all dynamic mappings
	if (FixedTiles.Num() == 0)
	{
		const int32 BeforeCount = FixedConstraint->GetFixedTileMappingsCount();
		FixedConstraint->ClearDynamicFixedTileMappings();
		const int32 AfterCount = FixedConstraint->GetFixedTileMappingsCount();
		
		UE_LOG(LogWFC, Log, TEXT("CLIENT: Cleared %d dynamic mappings (%d static preserved)"), 
			BeforeCount - AfterCount, AfterCount);
		UE_LOG(LogWFC, Log, TEXT("========================================"));
		return;
	}

	// Apply all received mappings
	int32 AppliedCount = 0;
	for (const FWFCFixedTileData& TileData : FixedTiles)
	{
		if (TileData.CellIndex != INDEX_NONE && TileData.TileId != INDEX_NONE)
		{
			FixedConstraint->SetFixedTileMapping(TileData.CellIndex, TileData.TileId);
			AppliedCount++;
		}
	}

	UE_LOG(LogWFC, Log, TEXT("CLIENT: Successfully applied %d/%d fixed tile mappings"), 
		AppliedCount, FixedTiles.Num());
	UE_LOG(LogWFC, Log, TEXT("CLIENT: Total fixed tiles now: %d"), 
		FixedConstraint->GetFixedTileMappingsCount());
	UE_LOG(LogWFC, Log, TEXT("========================================"));
}

void UWFCGeneratorComponent::MulticastSyncAndGenerate_Implementation(int32 Seed, const TArray<FWFCFixedTileData>& FixedTiles)
{
	// Only clients execute (server already has everything)
	if (!GetOwner() || GetOwner()->HasAuthority())
	{
		UE_LOG(LogWFC, Verbose, TEXT("Server received own MulticastSyncAndGenerate (ignored)"));
		return;
	}

	UE_LOG(LogWFC, Verbose, TEXT(""));
	UE_LOG(LogWFC, Verbose, TEXT("========================================"));
	UE_LOG(LogWFC, Log, TEXT("CLIENT: Received ATOMIC sync and generate command"));
	UE_LOG(LogWFC, Log, TEXT("Seed: %d, FixedTiles: %d"), Seed, FixedTiles.Num());
	UE_LOG(LogWFC, Verbose, TEXT("========================================"));

	if (!Generator)
	{
		UE_LOG(LogWFC, Error, TEXT("CLIENT: Cannot process - Generator is null!"));
		return;
	}

	// STEP 1: Clear old state and apply FixedTiles
	UWFCFixedTileConstraint* FixedConstraint = Generator->GetConstraint<UWFCFixedTileConstraint>();
	if (!FixedConstraint)
	{
		UE_LOG(LogWFC, Error, TEXT("CLIENT: Cannot sync - No FixedTileConstraint found!"));
		return;
	}

	// Clear previous dynamic mappings first
	const int32 BeforeCount = FixedConstraint->GetFixedTileMappingsCount();
	FixedConstraint->ClearDynamicFixedTileMappings();
	UE_LOG(LogWFC, Log, TEXT("CLIENT: Cleared %d previous dynamic mappings"), BeforeCount);

	// Special case: Empty array means clear only (no generation)
	if (FixedTiles.Num() == 0)
	{
		UE_LOG(LogWFC, Log, TEXT("CLIENT: Clear command received (no generation)"));
		UE_LOG(LogWFC, Log, TEXT("========================================"));
		return;
	}

	// Apply all new FixedTiles
	int32 AppliedCount = 0;
	for (const FWFCFixedTileData& TileData : FixedTiles)
	{
		if (TileData.CellIndex != INDEX_NONE && TileData.TileId != INDEX_NONE)
		{
			FixedConstraint->SetFixedTileMapping(TileData.CellIndex, TileData.TileId);
			AppliedCount++;
		}
	}

	UE_LOG(LogWFC, Log, TEXT("CLIENT: Applied %d/%d fixed tile mappings"), AppliedCount, FixedTiles.Num());
	UE_LOG(LogWFC, Log, TEXT("CLIENT: Total fixed tiles now: %d"), FixedConstraint->GetFixedTileMappingsCount());

	// STEP 2: Clean up old actors
	if (GetOwner()->IsA<AWFCTestingActor>())
	{
		static_cast<AWFCTestingActor*>(GetOwner())->DestroyAllSpawnedActors();
		UE_LOG(LogWFC, Log, TEXT("CLIENT: Destroyed old actors"));
	}

	// STEP 3: FORCE COMPLETE RE-INITIALIZATION
	// CRITICAL: Just like RunNewGeneration does, we need to fully reinitialize
	// Not just Reset(), because the constraints might have stale state
	UE_LOG(LogWFC, Warning, TEXT("CLIENT: Force reinitializing Generator to match server state"));
	
	// Force reinitialization (this will recreate constraints from scratch)
	Initialize(true);  // ✅ Force=true 完全重新初始化！
	
	// Now apply the FixedTiles AFTER reinitialization
	FixedConstraint = Generator->GetConstraint<UWFCFixedTileConstraint>();
	if (!FixedConstraint)
	{
		UE_LOG(LogWFC, Error, TEXT("CLIENT: FixedTileConstraint not found after reinitialization!"));
		return;
	}
	
	// Reapply all FixedTiles (both static from config and dynamic from server)
	AppliedCount = 0;
	for (const FWFCFixedTileData& TileData : FixedTiles)
	{
		if (TileData.CellIndex != INDEX_NONE && TileData.TileId != INDEX_NONE)
		{
			FixedConstraint->SetFixedTileMapping(TileData.CellIndex, TileData.TileId);
			AppliedCount++;
		}
	}
	
	UE_LOG(LogWFC, Log, TEXT("CLIENT: Reapplied %d/%d fixed tile mappings after reinitialization"), AppliedCount, FixedTiles.Num());
	UE_LOG(LogWFC, Log, TEXT("CLIENT: Total fixed tiles now: %d"), FixedConstraint->GetFixedTileMappingsCount());

	// STEP 4: Start generation with the new seed
	PrepareClientForGeneration(Seed);  // Use helper function

	UE_LOG(LogWFC, Log, TEXT("CLIENT: Starting generation with Seed %d and %d fixed tiles"), Seed, AppliedCount);

	// Verify generator has the updated FixedTileConstraint
	UWFCFixedTileConstraint* VerifyConstraint = Generator->GetConstraint<UWFCFixedTileConstraint>();
	if (VerifyConstraint)
	{
		UE_LOG(LogWFC, Log, TEXT("CLIENT: Verified FixedTileConstraint has %d mappings before generation"), 
			VerifyConstraint->GetFixedTileMappingsCount());
	}

	// Start generation
	StartGeneration();

	UE_LOG(LogWFC, Verbose, TEXT("========================================"));
}

int32 UWFCGeneratorComponent::GenerateNewSeed()
{
	// Generate a seed based on current time and world time
	int32 NewSeed = FMath::Rand();
	
	// Ensure seed is never 0
	if (NewSeed == 0)
	{
		NewSeed = 1;
	}

	UE_LOG(LogWFC, Log, TEXT("Generated new seed: %d"), NewSeed);
	return NewSeed;
}

void UWFCGeneratorComponent::PrepareClientForGeneration(int32 Seed)
{
	// Common logic for client preparation before generation
	if (GetOwner()->IsA<AWFCTestingActor>())
	{
		static_cast<AWFCTestingActor*>(GetOwner())->DestroyAllSpawnedActors();
	}
	
	RandomSeed = Seed;
	RandomStream.Initialize(RandomSeed);
	OnSeedReceivedEvent_BP.Broadcast(RandomSeed);
}

void UWFCGeneratorComponent::OnCellSelected(int32 CellIndex)
{
	OnCellSelectedEvent.Broadcast(CellIndex);
	OnCellSelectedEvent_BP.Broadcast(CellIndex);
}

void UWFCGeneratorComponent::OnStateChanged(EWFCGeneratorState State)
{
	OnStateChangedEvent_BP.Broadcast(State);

	if (State == EWFCGeneratorState::Finished || State == EWFCGeneratorState::Error)
	{
		if (State == EWFCGeneratorState::Finished)
		{
			UE_LOG(LogWFC, Log, TEXT("WFCGenerator finished successfully: %s"), *GetOwner()->GetName());
		}
		else
		{
			UE_LOG(LogWFC, Error, TEXT("WFCGenerator failed: %s"), *GetOwner()->GetName());
		}

		OnFinishedEvent_BP.Broadcast(State == EWFCGeneratorState::Finished);
		OnFinishedEvent.Broadcast(State);
	}
}

