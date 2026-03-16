// Copyright Bohdon Sayre. All Rights Reserved.

#include "WFCTestingActor.h"

#include "WFCGeneratorComponent.h"
#include "WFCTileActorBase.h"
#include "WFCModule.h"
#include "Core/WFCGenerator.h"
#include "Core/WFCGrid.h"
#include "Core/WFCModel.h"
#include "Core/Constraints/WFCFixedTileConstraint.h"
#include "LevelInstance/WFCTileLevelInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"

AWFCTestingActor::AWFCTestingActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	  bLoadTileLevelsOnFinish(false),
	  bSpawnOnError(true),
	  bSpawnOnCellSelection(false),
	  bLoadTileLevelsOnSelection(false),
	  bEnableMultiplayer(false),
	  bServerOnlyGeneration(true),
	  ServerGenerationDelay(1.0f),
	WFC_Seed(0)
{
	WFCGenerator = CreateDefaultSubobject<UWFCGeneratorComponent>(TEXT("WFCGenerator"));
	RootComponent = WFCGenerator;

	// Enable replication for multiplayer support
	bReplicates = true;
	bAlwaysRelevant = true;
}

const FWFCModelAssetTile* AWFCTestingActor::GetAssetTileForCell(int32 CellIndex) const
{
	const UWFCGenerator* Generator = WFCGenerator->GetGenerator();
	if (!Generator || !Generator->IsValidCellIndex(CellIndex))
	{
		return nullptr;
	}

	const FWFCCell& Cell = Generator->GetCell(CellIndex);
	if (!Cell.HasSelection())
	{
		return nullptr;
	}

	const FWFCTileId SelectedTileId = Cell.GetSelectedTileId();
	if (!Generator->IsValidTileId(SelectedTileId))
	{
		return nullptr;
	}

	return Generator->GetModel()->GetTile<FWFCModelAssetTile>(SelectedTileId);
}

FTransform AWFCTestingActor::GetCellTransform(int32 CellIndex, int32 Rotation) const
{
	FTransform ActorTransform = GetActorTransform();

	if (!WFCGenerator->IsInitialized())
	{
		return ActorTransform;
	}

	const UWFCGrid* Grid = WFCGenerator->GetGrid();
	if (!Grid || !Grid->IsValidCellIndex(CellIndex))
	{
		return ActorTransform;
	}

	return Grid->GetCellWorldTransform(CellIndex, Rotation) * ActorTransform;
}

void AWFCTestingActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (GetWorld() && GetWorld()->IsGameWorld())
	{
		WFCGenerator->OnCellSelectedEvent.AddUObject(this, &AWFCTestingActor::OnCellSelected);
		WFCGenerator->OnFinishedEvent.AddUObject(this, &AWFCTestingActor::OnGeneratorFinished);
		WFCGenerator->OnSeedReceivedEvent_BP.AddDynamic(this, &AWFCTestingActor::OnSeedReceived);
	}
}

void AWFCTestingActor::BeginPlay()
{
	Super::BeginPlay();

	// Configure multiplayer settings
	if (bEnableMultiplayer)
	{
		ConfigureMultiplayer(true, !HasAuthority());
		
		// CRITICAL: Initialize Generator on clients too (even if not auto-running)
		// This ensures Generator exists when RPC arrives from server
		if (!HasAuthority() && WFCGenerator && !WFCGenerator->IsInitialized())
		{
			UE_LOG(LogWFC, Log, TEXT("CLIENT: Pre-initializing Generator for RPC support"));
			if (!WFCGenerator->Initialize())
			{
				UE_LOG(LogWFC, Error, TEXT("CLIENT: Failed to initialize Generator! RPC will not work properly."));
				// Disable multiplayer to prevent crash
				bEnableMultiplayer = false;
				return;
			}
		}
		
		// If this is the server and we want server-only generation
		if (HasAuthority() && bServerOnlyGeneration)
		{
			// Disable auto-run on the component, we'll handle it manually
			WFCGenerator->bAutoRun = false;
			
			// Start generation after a delay to allow clients to connect
			if (ServerGenerationDelay > 0.0f)
			{
				GetWorld()->GetTimerManager().SetTimer(
					ServerGenerationTimerHandle,
					this,
					&AWFCTestingActor::DelayedServerGeneration,
					ServerGenerationDelay,
					false
				);
			}
			else
			{
				DelayedServerGeneration();
			}
		}
	}
}

void AWFCTestingActor::ConfigureMultiplayer(bool bEnable, bool bRequestFromServer)
{
	if (WFCGenerator)
	{
		WFCGenerator->bUseMultiplayerSeeds = bEnable;
		WFCGenerator->bRequestSeedFromServer = bRequestFromServer;
		
		UE_LOG(LogWFC, Log, TEXT("Configured multiplayer: Enable=%s, RequestFromServer=%s"), 
			bEnable ? TEXT("true") : TEXT("false"),
			bRequestFromServer ? TEXT("true") : TEXT("false"));
	}
}

void AWFCTestingActor::ServerStartGeneration(int32 Seed)
{
	if (!HasAuthority())
	{
		UE_LOG(LogWFC, Warning, TEXT("ServerStartGeneration called on client, ignoring"));
		return;
	}

	// 使用辅助函数生成有效种子
	const int32 EffectiveSeed = GenerateValidSeed(Seed);

	UE_LOG(LogWFC, Error, TEXT("=== SERVER STARTING GENERATION WITH SEED: %d ==="), EffectiveSeed);

	if (WFCGenerator)
	{
		// 确保多人游戏设置正确
		WFCGenerator->bUseMultiplayerSeeds = true;
		WFCGenerator->SetSeedAndRun(EffectiveSeed);
	}
}

int32 AWFCTestingActor::GetCurrentSeed() const
{
	return WFCGenerator ? WFCGenerator->GetRandomSeed() : 0;
}

void AWFCTestingActor::RunNewGeneration(int32 NewSeed)
{
	UE_LOG(LogWFC, Warning, TEXT("=== RunNewGeneration called with seed: %d ==="), NewSeed);

	// ? 恢复 Actor 创建标志（正常生成总是创建 Actor）
	bShouldSpawnActorsOnFinish = true;

	// 清理之前生成的Actor
	DestroyAllSpawnedActors();

	// 使用辅助函数生成有效种子
	const int32 EffectiveSeed = GenerateValidSeed(NewSeed);
	UE_LOG(LogWFC, Log, TEXT("Using effective seed: %d"), EffectiveSeed);

	if (bEnableMultiplayer)
	{
		if (HasAuthority())
		{
			// 服务器：使用确定性生成并同步到客户端
			UE_LOG(LogWFC, Log, TEXT("Server: Starting deterministic generation with seed %d"), EffectiveSeed);
			ServerStartDeterministicGeneration(EffectiveSeed);
		}
		else
		{
			// 客户端：请求服务器使用指定种子开始生成
			UE_LOG(LogWFC, Log, TEXT("Client: Requesting server to start generation with seed %d"), EffectiveSeed);
			ServerRequestDeterministicGeneration(EffectiveSeed);
		}
	}
	else
	{
		// 单人游戏模式：直接使用现有的WFC生成功能
		if (WFCGenerator)
		{
			UE_LOG(LogWFC, Log, TEXT("Single-player: Starting generation with seed %d"), EffectiveSeed);
			WFCGenerator->SetSeedAndRun(EffectiveSeed);
		}
		else
		{
			UE_LOG(LogWFC, Error, TEXT("WFCGenerator is not available"));
		}
	}
}

void AWFCTestingActor::RunNewGenerationServerOnly(int32 NewSeed, bool bSpawnActors)
{
	// 使用辅助函数生成有效种子
	//const int32 EffectiveSeed = GenerateValidSeed(NewSeed);
	
	// ? 保存种子到公共变量
	WFC_Seed = NewSeed;
	
	if (HasAuthority())
	{
		UE_LOG(LogWFC, Verbose, TEXT("SERVER-ONLY: Starting generation with seed %d (SpawnActors=%s)"), 
			WFC_Seed, bSpawnActors ? TEXT("true") : TEXT("false"));

		// ? 根据参数设置是否生成 Actor
		bShouldSpawnActorsOnFinish = bSpawnActors;

		// 运行纯数据计算
		if (WFCGenerator)
		{
			WFCGenerator->SetSeedAndRun(WFC_Seed);
		}

		UE_LOG(LogWFC, Verbose, TEXT("SERVER-ONLY: Started generation (actors will %s spawned)"), 
			bSpawnActors ? TEXT("be") : TEXT("NOT be"));
	}
	else
	{
		// CLIENT: Initialize Generator for future RPC
		UE_LOG(LogWFC, Log, TEXT("CLIENT: RunNewGenerationServerOnly - Ensuring Generator is initialized"));
		
		if (WFCGenerator && !WFCGenerator->IsInitialized())
		{
			UE_LOG(LogWFC, Log, TEXT("CLIENT: Initializing Generator for future RPC..."));
			if (!WFCGenerator->Initialize())
			{
				UE_LOG(LogWFC, Error, TEXT("CLIENT: Failed to initialize Generator for RPC!"));
				return;
			}
		}
	}
}

void AWFCTestingActor::SyncFinalGenerationToClients()
{
	if (!HasAuthority())
	{
		UE_LOG(LogWFC, Warning, TEXT("SyncFinalGenerationToClients: Can only be called on server"));
		return;
	}

	if (!WFCGenerator || !WFCGenerator->GetGenerator())
	{
		UE_LOG(LogWFC, Error, TEXT("SyncFinalGenerationToClients: WFCGenerator is not available"));
		return;
	}

	const int32 FinalSeed = WFCGenerator->GetRandomSeed();

	UE_LOG(LogWFC, Verbose, TEXT(""));
	UE_LOG(LogWFC, Verbose, TEXT("========================================"));
	UE_LOG(LogWFC, Log, TEXT("SERVER: Syncing FINAL generation to clients"));
	UE_LOG(LogWFC, Log, TEXT("Final Seed: %d"), FinalSeed);
	UE_LOG(LogWFC, Verbose, TEXT("========================================"));

	// Get all FixedTileMappings
	UWFCGenerator* Generator = WFCGenerator->GetGenerator();
	UWFCFixedTileConstraint* FixedConstraint = Generator->GetConstraint<UWFCFixedTileConstraint>();

	TArray<FWFCFixedTileData> AllFixedTiles;

	if (FixedConstraint)
	{
		const int32 TotalMappings = FixedConstraint->GetFixedTileMappingsCount();
		AllFixedTiles.Reserve(TotalMappings);

		const int32 NumCells = Generator->GetNumCells();
		for (int32 CellIndex = 0; CellIndex < NumCells; ++CellIndex)
		{
			const FWFCTileId* TileIdPtr = FixedConstraint->GetFixedTileMapping(CellIndex);
			if (TileIdPtr && *TileIdPtr != INDEX_NONE)
			{
				AllFixedTiles.Add(FWFCFixedTileData(CellIndex, *TileIdPtr));
			}
		}

		const int32 DataSizeBytes = AllFixedTiles.Num() * sizeof(FWFCFixedTileData);
		const float DataSizeKB = DataSizeBytes / 1024.0f;

		UE_LOG(LogWFC, Log, TEXT("SERVER: Collected %d total fixed tiles"), AllFixedTiles.Num());
		UE_LOG(LogWFC, Log, TEXT("SERVER: Data size: %d bytes (%.2f KB)"), DataSizeBytes, DataSizeKB);
	}
	else
	{
		UE_LOG(LogWFC, Warning, TEXT("SERVER: No FixedTileConstraint found - syncing seed only"));
	}

		// ? 新增：在服务器端生成Actor
	UE_LOG(LogWFC, Log, TEXT("SERVER: Spawning actors for all cells..."));
	SpawnActorsForAllCells();
	const int32 SpawnedCount = SpawnedTileActors.Num();
	UE_LOG(LogWFC, Log, TEXT("SERVER: Spawned %d tile actors"), SpawnedCount);

	if (bLoadTileLevelsOnFinish)
	{
		LoadAllTileActorLevels();
	}

	// 同步到客户端
	WFCGenerator->MulticastSyncAndGenerate(FinalSeed, AllFixedTiles);

	UE_LOG(LogWFC, Warning, TEXT("SERVER: Sent ATOMIC sync+generate RPC to clients"));
	UE_LOG(LogWFC, Warning, TEXT("Clients will: Clear old tiles → Apply new tiles → Generate → Spawn actors"));
	UE_LOG(LogWFC, Warning, TEXT("========================================"));
}

void AWFCTestingActor::DelayedServerGeneration()
{
	if (HasAuthority())
	{
		UE_LOG(LogWFC, Log, TEXT("Starting delayed server generation"));
		ServerStartGeneration();
	}
}

void AWFCTestingActor::OnCellSelected(int32 CellIndex)
{
	// ? 同时检查两个标志
	if (bSpawnOnCellSelection && bShouldSpawnActorsOnFinish)
	{
		SpawnActorForCell(CellIndex, bLoadTileLevelsOnSelection);
	}
}

void AWFCTestingActor::OnGeneratorFinished(EWFCGeneratorState State)
{
	// ? 只在标志允许时才创建 Actor
	if (bShouldSpawnActorsOnFinish)
	{
		if (State == EWFCGeneratorState::Finished ||
			(State == EWFCGeneratorState::Error && bSpawnOnError))
		{
			// spawn tile actors (if they're not already)
			SpawnActorsForAllCells();

			if (bLoadTileLevelsOnFinish)
			{
				// ensure all tile levels are loaded
				LoadAllTileActorLevels();
			}
		}
	}

	// 添加调试信息用于多人游戏同步验证
	if (bEnableMultiplayer)
	{
		const TCHAR* NetworkRole = GetNetworkRoleString();
		UE_LOG(LogWFC, Warning, TEXT("=== %s WFC FINISHED: State=%d, Seed=%d ==="), 
			NetworkRole, (int32)State, GetCurrentSeed());

		// 如果成功完成，计算结果哈希进行验证
		if (State == EWFCGeneratorState::Finished)
		{
			// 使用缓存数组避免重复分配
			TempResultsArray.Reset();
			WFCGenerator->GetSelectedTileIds(TempResultsArray);
			
			uint32 Hash = 0;
			const int32 HashCount = FMath::Min(10, TempResultsArray.Num());
			for (int32 i = 0; i < HashCount; ++i)
			{
				Hash = HashCombine(Hash, GetTypeHash(TempResultsArray[i]));
			}
			
			UE_LOG(LogWFC, Error, TEXT("=== %s RESULT HASH: 0x%08X ==="), NetworkRole, Hash);
			
			// 记录前5个瓦片选择以便比较
			if (TempResultsArray.Num() >= 5)
			{
				UE_LOG(LogWFC, Error, TEXT("=== %s FIRST 5 TILES: [%d,%d,%d,%d,%d] ==="), 
					NetworkRole, TempResultsArray[0], TempResultsArray[1], TempResultsArray[2], TempResultsArray[3], TempResultsArray[4]);
			}
		}
	}
	else
	{
		UE_LOG(LogWFC, Log, TEXT("WFC generation completed with state: %d"), (int32)State);
	}
}

void AWFCTestingActor::OnSeedReceived(int32 Seed)
{
	const TCHAR* NetworkRole = GetNetworkRoleString();
	UE_LOG(LogWFC, Error, TEXT("=== %s SEED RECEIVED: %d ==="), NetworkRole, Seed);
}

void AWFCTestingActor::ServerRequestDeterministicGeneration_Implementation(int32 Seed)
{
	UE_LOG(LogWFC, Log, TEXT("Client requested deterministic generation with seed: %d"), Seed);
	
	// 服务器接收到客户端请求后，启动确定性生成
	ServerStartDeterministicGeneration(Seed);
}

bool AWFCTestingActor::ServerRequestDeterministicGeneration_Validate(int32 Seed)
{
	// 验证种子值是否合理
	return Seed >= 0;
}

void AWFCTestingActor::ServerStartDeterministicGeneration(int32 Seed)
{
	if (!HasAuthority())
	{
		UE_LOG(LogWFC, Warning, TEXT("ServerStartDeterministicGeneration called on client, ignoring"));
		return;
	}

	// Seed already validated by caller (RunNewGeneration)
	UE_LOG(LogWFC, Error, TEXT("=== SERVER STARTING DETERMINISTIC GENERATION WITH SEED: %d ==="), Seed);

	// 通知所有客户端开始确定性生成
	MulticastStartDeterministicGeneration(Seed);

	// 服务器也执行确定性生成
	if (WFCGenerator)
	{
		WFCGenerator->SetSeedAndRun(Seed);
	}
}

void AWFCTestingActor::MulticastStartDeterministicGeneration_Implementation(int32 Seed)
{
	// 只有客户端执行（服务器已经在ServerStartDeterministicGeneration中执行过了）
	if (!HasAuthority())
	{
		UE_LOG(LogWFC, Warning, TEXT("=== CLIENT STARTING DETERMINISTIC GENERATION WITH SEED: %d ==="), Seed);

		// 清理之前生成的Actor
		DestroyAllSpawnedActors();

		if (WFCGenerator)
		{
			WFCGenerator->SetSeedAndRun(Seed);
		}
	}
	else
	{
		UE_LOG(LogWFC, Verbose, TEXT("SERVER received own multicast deterministic generation call (ignored)"));
	}
}

int32 AWFCTestingActor::GetGenerationResultHash() const
{
	if (WFCGenerator)
	{
		// 获取生成结果并计算简单哈希
		TArray<int32> TileIds;
		WFCGenerator->GetSelectedTileIds(TileIds);
		
		uint32 Hash = 0;
		const int32 HashCount = FMath::Min(10, TileIds.Num());
		for (int32 i = 0; i < HashCount; ++i)
		{
			Hash = HashCombine(Hash, GetTypeHash(TileIds[i]));
		}
		return (int32)Hash;
	}
	return 0;
}

void AWFCTestingActor::TestDeterministicGeneration(int32 TestSeed, int32 TestIterations)
{
	// 使用核心逻辑，不在完成时生成Actor
	RunDeterministicGenerationCore(TestSeed, TestIterations, false);
}

bool AWFCTestingActor::ValidateDeterministicGeneration(int32 TestSeed, int32 TestIterations)
{
	// 使用核心逻辑，在完成时生成Actor用于显示
	return RunDeterministicGenerationCore(TestSeed, TestIterations, true);
}

AActor* AWFCTestingActor::SpawnActorForCell(int32 CellIndex, bool bAutoLoad)
{
	const FWFCModelAssetTile* AssetTile = GetAssetTileForCell(CellIndex);
	if (!AssetTile)
	{
		return nullptr;
	}

	const UWFCTileAsset* TileAsset = Cast<UWFCTileAsset>(AssetTile->TileAsset.Get());
	if (!TileAsset)
	{
		return nullptr;
	}

	TWeakObjectPtr<AActor> AlreadySpawnedActor = SpawnedTileActors.FindRef(CellIndex);
	if (AlreadySpawnedActor.IsValid())
	{
		// actor already spawned for this cell
		return AlreadySpawnedActor.Get();
	}

	const TSubclassOf<AActor> ActorClass = TileAsset->GetTileDefActorClass(AssetTile->TileDefIndex);
	if (!ActorClass)
	{
		return nullptr;
	}

	const FTransform Transform = GetCellTransform(CellIndex, AssetTile->Rotation);

	// ============================================================================
	// MULTIPLAYER NET STARTUP ACTOR SPAWNING (Alvaro Method)
	// ============================================================================
	// Requirements for Net Startup Actors:
	// 1. Deterministic naming (same on server and client)
	// 2. Set bNetStartup = true (treat as level-loaded)
	// 3. Spawn before replication starts (or defer relevance)
	
	FActorSpawnParameters SpawnParams;
	
	// === Deterministic Naming ===
	// Use CellIndex to create stable, deterministic names
	// Format: "WFCTile_<CellIndex>_<TileId>_<Rotation>"
	const FString DeterministicName = FString::Printf(TEXT("WFCTile_%d_%d_%d"), 
		CellIndex, AssetTile->Id, AssetTile->Rotation);
	
	SpawnParams.Name = FName(*DeterministicName);
	SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Required_Fatal;
	
	// === Spawn Collision Handling ===
	// Always spawn even if overlapping (procedural generation needs this)
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	// === Deferred Construction ===
	// Allow setting bNetStartup before FinishSpawning
	SpawnParams.bDeferConstruction = true;
	
	// === Remove RF_Transient ===
	// Net Startup Actors must NOT be transient
	// SpawnParams.ObjectFlags = RF_Transient; // ? Remove this!
	
	// Spawn the actor (deferred)
	AActor* TileActor = GetWorld()->SpawnActor<AActor>(ActorClass, Transform, SpawnParams);
	if (!TileActor)
	{
		return nullptr;
	}

	// ============================================================================
	// CRITICAL: Set bNetStartup = true
	// ============================================================================
	// This tells UE to treat this Actor as if it was loaded from the level,
	// making it "authoritative" on both server and client.
	TileActor->bNetStartup = true;
	
	// Add tag to identify procedurally generated Actors
	TileActor->Tags.Add(TEXT("WFCProcGen"));
	
	// Set procedural generation flag if Actor is AWFCTileActorBase
	if (AWFCTileActorBase* WFCTileActor = Cast<AWFCTileActorBase>(TileActor))
	{
		WFCTileActor->bIsProcedurallyGenerated = true;
		WFCTileActor->WFCCellIndex = CellIndex;
	}
	
	// Finish spawning (applies transform and completes construction)
	TileActor->FinishSpawning(Transform);

	SpawnedTileActors.Add(CellIndex, TileActor);

	// handle level instance tiles
	if (AWFCTileLevelInstance* LevelInstanceTile = Cast<AWFCTileLevelInstance>(TileActor))
	{
		LevelInstanceTile->bAutoLoad = bAutoLoad;
		LevelInstanceTile->SetGeneratorComp(WFCGenerator);
		LevelInstanceTile->SetTileAndCell(AssetTile, CellIndex);
	}

	UE_LOG(LogWFC, Verbose, TEXT("Spawned Net Startup Actor: %s (Cell %d)"), *DeterministicName, CellIndex);

	return TileActor;
}

AActor* AWFCTestingActor::GetActorForCell(int32 CellIndex) const
{
	return SpawnedTileActors.FindRef(CellIndex).Get();
}

void AWFCTestingActor::SpawnActorsForAllCells()
{
	const UWFCGenerator* Generator = WFCGenerator->GetGenerator();
	if (!Generator)
	{
		return;
	}

	for (int32 CellIndex = 0; CellIndex < Generator->GetNumCells(); ++CellIndex)
	{
		SpawnActorForCell(CellIndex, false);
	}
}

void AWFCTestingActor::LoadAllTileActorLevels()
{
	for (const auto& Elem : SpawnedTileActors)
	{
		TWeakObjectPtr<AActor> TileActor = Elem.Value;
		if (AWFCTileLevelInstance* LevelInstanceTile = Cast<AWFCTileLevelInstance>(TileActor.Get()))
		{
			LevelInstanceTile->LoadLevel();
		}
	}
}

void AWFCTestingActor::DestroyAllSpawnedActors()
{
	if (SpawnedTileActors.IsEmpty())
	{
		return;
	}

	// 优化：批量收集需要销毁的Actor，然后一次性销毁
	TArray<AActor*> ActorsToDestroy;
	ActorsToDestroy.Reserve(SpawnedTileActors.Num());

	for (auto& Elem : SpawnedTileActors)
	{
		if (Elem.Value.IsValid())
		{
			ActorsToDestroy.Add(Elem.Value.Get());
		}
	}

	// 批量销毁Actor以提高性能
	for (AActor* Actor : ActorsToDestroy)
	{
		Actor->Destroy();
	}

	SpawnedTileActors.Reset();

	UE_LOG(LogWFC, Log, TEXT("Destroyed %d spawned tile actors"), ActorsToDestroy.Num());
}

// Helper function implementations
int32 AWFCTestingActor::GenerateValidSeed(int32 InputSeed) const
{
	if (InputSeed != 0)
	{
		return InputSeed;
	}

	int32 NewSeed = FMath::Rand();
	if (NewSeed == 0)
	{
		NewSeed = 1;
	}
	return NewSeed;
}

bool AWFCTestingActor::WaitForGenerationCompletion(int32 TimeoutMs) const
{
	if (!WFCGenerator)
	{
		return false;
	}

	int32 WaitCount = 0;
	const int32 MaxWaitCount = TimeoutMs / 10; // 10ms sleep intervals
	
while (WFCGenerator->GetState() == EWFCGeneratorState::InProgress && WaitCount < MaxWaitCount)
	{
		FPlatformProcess::Sleep(0.01f);
		++WaitCount;
	}
	
	return WFCGenerator->GetState() == EWFCGeneratorState::Finished;
}

bool AWFCTestingActor::RunDeterministicGenerationCore(int32 TestSeed, int32 TestIterations, bool bSpawnActorsOnCompletion)
{
	if (!WFCGenerator || !WFCGenerator->GetGenerator())
	{
		UE_LOG(LogWFC, Error, TEXT("WFCGenerator is not available for testing"));
		return false;
	}

	UE_LOG(LogWFC, Warning, TEXT("=== RUNNING DETERMINISTIC GENERATION: Seed=%d, Iterations=%d ==="), 
		TestSeed, TestIterations);

	TArray<int32> ResultHashes;
	ResultHashes.Reserve(TestIterations);
	
	for (int32 i = 0; i < TestIterations; ++i)
	{
		UE_LOG(LogWFC, Log, TEXT("Test iteration %d/%d"), i + 1, TestIterations);
		
		// 运行生成
		RunNewGeneration(TestSeed);
		
		// 等待生成完成
		if (!WaitForGenerationCompletion())
		{
			UE_LOG(LogWFC, Error, TEXT("Generation failed or timed out in iteration %d"), i + 1);
			return false;
		}
		
		int32 Hash = GetGenerationResultHash();
		ResultHashes.Add(Hash);
		UE_LOG(LogWFC, Log, TEXT("Iteration %d result hash: 0x%08X"), i + 1, Hash);
	}
	
	// 验证所有结果是否一致
	bool bAllMatch = true;
	if (ResultHashes.Num() > 0)
	{
		const int32 FirstHash = ResultHashes[0];
		for (int32 i = 1; i < ResultHashes.Num(); ++i)
		{
			if (ResultHashes[i] != FirstHash)
			{
				bAllMatch = false;
				UE_LOG(LogWFC, Error, TEXT("Hash mismatch: First=0x%08X, Iteration %d=0x%08X"), 
					FirstHash, i + 1, ResultHashes[i]);
			}
		}
		
		if (bAllMatch)
		{
			UE_LOG(LogWFC, Warning, TEXT("=== DETERMINISTIC GENERATION TEST PASSED: All %d iterations produced identical results (Hash=0x%08X) ==="), 
				TestIterations, FirstHash);
			
			// 如果需要，在最后一次生成后创建Actor用于显示
			if (bSpawnActorsOnCompletion)
			{
				SpawnActorsForAllCells();
				if (bLoadTileLevelsOnFinish)
				{
					LoadAllTileActorLevels();
				}
			}
		}
		else
		{
			UE_LOG(LogWFC, Error, TEXT("=== DETERMINISTIC GENERATION TEST FAILED: Results were inconsistent ==="));
		}
	}

	return bAllMatch;
}

void AWFCTestingActor::ClearDynamicFixedTiles()
{
	if (!WFCGenerator || !WFCGenerator->GetGenerator())
	{
		UE_LOG(LogWFC, Warning, TEXT("ClearDynamicFixedTiles: WFCGenerator is not available"));
		return;
	}

	UWFCGenerator* Generator = WFCGenerator->GetGenerator();
	UWFCFixedTileConstraint* FixedConstraint = Generator->GetConstraint<UWFCFixedTileConstraint>();
	
	if (!FixedConstraint)
	{
		UE_LOG(LogWFC, Warning, TEXT("ClearDynamicFixedTiles: No FixedTileConstraint found"));
		return;
	}

	// MULTIPLAYER SYNCHRONIZATION
	if (bEnableMultiplayer && GetWorld()->GetNetMode() != NM_Standalone)
	{
		if (HasAuthority())
		{
			// ? SERVER: Clear locally first
			const int32 BeforeCount = FixedConstraint->GetFixedTileMappingsCount();
			FixedConstraint->ClearDynamicFixedTileMappings();
			const int32 AfterCount = FixedConstraint->GetFixedTileMappingsCount();
			
			UE_LOG(LogWFC, Warning, TEXT("========================================"));
			UE_LOG(LogWFC, Warning, TEXT("SERVER: Clearing dynamic fixed tiles"));
			UE_LOG(LogWFC, Warning, TEXT("Cleared %d dynamic mappings (%d static preserved)"), 
				BeforeCount - AfterCount, AfterCount);
			
			// ? Send clear command to all clients
			TArray<FWFCFixedTileData> EmptyFixedTiles;
			WFCGenerator->MulticastSyncFixedTiles(EmptyFixedTiles);
			
			UE_LOG(LogWFC, Warning, TEXT("SERVER: Sent CLEAR command to all clients via MulticastSyncFixedTiles"));
			UE_LOG(LogWFC, Warning, TEXT("Current seed: %d (preserved for next generation)"), 
				WFCGenerator->GetRandomSeed());
			UE_LOG(LogWFC, Warning, TEXT("========================================"));
		}
		else
		{
			// ? CLIENT: Should NOT directly call ClearDynamicFixedTiles
			// Wait for server's RPC instead
			UE_LOG(LogWFC, Error, TEXT("CLIENT: ClearDynamicFixedTiles called on client - this should only be called on server!"));
			UE_LOG(LogWFC, Error, TEXT("Wait for server's RPC to clear tiles. Ignoring local call."));
		}
	}
	else
	{
		// ? SINGLE PLAYER: Direct clear (no synchronization needed)
		const int32 BeforeCount = FixedConstraint->GetFixedTileMappingsCount();
		FixedConstraint->ClearDynamicFixedTileMappings();
		const int32 AfterCount = FixedConstraint->GetFixedTileMappingsCount();
		
		UE_LOG(LogWFC, Log, TEXT("Single-player: Cleared %d dynamic mappings (%d static preserved)"), 
			BeforeCount - AfterCount, AfterCount);
	}
}

bool AWFCTestingActor::ClearAndSyncDynamicFixedTiles()
{
	if (!HasAuthority())
	{
		UE_LOG(LogWFC, Error, TEXT("ClearAndSyncDynamicFixedTiles: Can only be called on server!"));
		return false;
	}

	if (!WFCGenerator || !WFCGenerator->GetGenerator())
	{
		UE_LOG(LogWFC, Error, TEXT("ClearAndSyncDynamicFixedTiles: WFCGenerator is not available"));
		return false;
	}

	UWFCGenerator* Generator = WFCGenerator->GetGenerator();
	UWFCFixedTileConstraint* FixedConstraint = Generator->GetConstraint<UWFCFixedTileConstraint>();
	
	if (!FixedConstraint)
	{
		UE_LOG(LogWFC, Error, TEXT("ClearAndSyncDynamicFixedTiles: No FixedTileConstraint found"));
		return false;
	}

	UE_LOG(LogWFC, Warning, TEXT(""));
	UE_LOG(LogWFC, Warning, TEXT("========================================"));
	UE_LOG(LogWFC, Warning, TEXT("SERVER: CLEARING AND SYNCING DYNAMIC FIXED TILES"));
	UE_LOG(LogWFC, Warning, TEXT("========================================"));

	// Step 1: Get current state
	const int32 BeforeCount = FixedConstraint->GetFixedTileMappingsCount();
	TArray<FWFCFixedTileConstraintEntry> DynamicMappings;
	FixedConstraint->GetDynamicMappings(DynamicMappings);
	const int32 DynamicCount = DynamicMappings.Num();
	const int32 StaticCount = BeforeCount - DynamicCount;

	UE_LOG(LogWFC, Log, TEXT("Before clear: %d total mappings (%d static + %d dynamic)"), 
		BeforeCount, StaticCount, DynamicCount);

	// Step 2: Clear locally
	FixedConstraint->ClearDynamicFixedTileMappings();
	const int32 AfterCount = FixedConstraint->GetFixedTileMappingsCount();

	UE_LOG(LogWFC, Log, TEXT("After clear: %d total mappings (%d static preserved, %d dynamic removed)"), 
		AfterCount, StaticCount, DynamicCount);

	// Step 3: Destroy old actors to ensure clean state
	if (DynamicCount > 0)
	{
		UE_LOG(LogWFC, Log, TEXT("Destroying %d spawned actors for clean state..."), SpawnedTileActors.Num());
		DestroyAllSpawnedActors();
	}

	// Step 4: Sync to all clients using the ATOMIC RPC
	// This ensures clients clear their state AND prepare for next generation
	TArray<FWFCFixedTileData> EmptyFixedTiles;  // Empty array signals "clear only, no generation"
	const int32 CurrentSeed = WFCGenerator->GetRandomSeed();

	// ?? CRITICAL: We use MulticastSyncAndGenerate with empty array
	// This will trigger client-side cleanup but NOT start generation
	// (See implementation in WFCGeneratorComponent.cpp - empty array = clear only)
	WFCGenerator->MulticastSyncAndGenerate(CurrentSeed, EmptyFixedTiles);

	UE_LOG(LogWFC, Warning, TEXT("SERVER: Sent ATOMIC clear command to all clients"));
	UE_LOG(LogWFC, Warning, TEXT("Clients will: Clear old actors → Reset Generator → Clear dynamic tiles"));
	UE_LOG(LogWFC, Warning, TEXT("Current seed preserved: %d"), CurrentSeed);
	UE_LOG(LogWFC, Warning, TEXT(""));
	UE_LOG(LogWFC, Warning, TEXT("? Ready for new iterative fixing session"));
	UE_LOG(LogWFC, Warning, TEXT("========================================"));

	return true;
}

void AWFCTestingActor::PrintGenerationState()
{
	const TCHAR* NetworkRole = GetNetworkRoleString();
	
	UE_LOG(LogWFC, Warning, TEXT(""));
	UE_LOG(LogWFC, Warning, TEXT("========================================"));
	UE_LOG(LogWFC, Warning, TEXT("%s GENERATION STATE"), NetworkRole);
	UE_LOG(LogWFC, Warning, TEXT("========================================"));
	
	if (!WFCGenerator)
	{
		UE_LOG(LogWFC, Error, TEXT("WFCGenerator: NULL"));
		return;
	}
	
	UE_LOG(LogWFC, Log, TEXT("Generator Initialized: %s"), WFCGenerator->IsInitialized() ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogWFC, Log, TEXT("Generator State: %d"), (int32)WFCGenerator->GetState());
	UE_LOG(LogWFC, Log, TEXT("Current Seed: %d"), GetCurrentSeed());
	UE_LOG(LogWFC, Log, TEXT("Spawned Actors: %d"), SpawnedTileActors.Num());
	
	UWFCGenerator* Generator = WFCGenerator->GetGenerator();
	if (Generator)
	{
		UE_LOG(LogWFC, Log, TEXT("Generator Cells: %d"), Generator->GetNumCells());
		
		// Check FixedTileConstraint
		UWFCFixedTileConstraint* FixedConstraint = Generator->GetConstraint<UWFCFixedTileConstraint>();
		if (FixedConstraint)
		{
			const int32 TotalMappings = FixedConstraint->GetFixedTileMappingsCount();
			UE_LOG(LogWFC, Log, TEXT("FixedTileConstraint Mappings: %d"), TotalMappings);
		}
		else
		{
			UE_LOG(LogWFC, Warning, TEXT("FixedTileConstraint: NOT FOUND"));
		}
		
		// Get generation result hash
		int32 Hash = GetGenerationResultHash();
		UE_LOG(LogWFC, Log, TEXT("Result Hash: 0x%08X"), Hash);
	}
	else
	{
		UE_LOG(LogWFC, Error, TEXT("Generator: NULL"));
	}
	
	UE_LOG(LogWFC, Warning, TEXT("========================================"));
}

void AWFCTestingActor::MulticastCleanupUnreachableTiles_Implementation(FIntVector StartLocation, const FGameplayTagContainer& PassableEdgeTags, const FGameplayTagContainer& RetainTags)
{
	UE_LOG(LogWFC, Log, TEXT("MulticastCleanupUnreachableTiles: Pathfinding removed, no cleanup performed."));
}

int32 AWFCTestingActor::ExportAllFixedTiles(TArray<FWFCFixedTileData>& OutFixedTiles) const
{
	OutFixedTiles.Reset();
	return 0;
}

bool AWFCTestingActor::ImportAndApplyFixedTiles(const TArray<FWFCFixedTileData>& FixedTiles)
{
	return false;
}
