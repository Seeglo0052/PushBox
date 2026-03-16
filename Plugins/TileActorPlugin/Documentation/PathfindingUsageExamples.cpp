// 使用示例：如何在你的项目中设置 WFC 寻路约束以确保迷宫可解

#include "WFCGeneratorComponent.h"
#include "Core/Constraints/WFCPathfindingConstraint.h"
#include "WFCPathfindingBlueprintLibrary.h"

// ============================================================================
// 示例 1: C++ 中添加寻路约束 - 简单 2D 迷宫
// ============================================================================
void SetupSimple2DMaze(UWFCGeneratorComponent* WFCGenerator)
{
	if (!WFCGenerator || !WFCGenerator->GetGenerator())
	{
		return;
	}

	// 创建寻路约束
	UWFCPathfindingConstraint* PathConstraint = NewObject<UWFCPathfindingConstraint>(
		WFCGenerator->GetGenerator()
	);

	// 启用寻路验证
	PathConstraint->bEnablePathfinding = true;

	// 定义"可通过"的边缘标签（必须与你的 Tile 资产中的标签匹配）
	PathConstraint->PassableEdgeTags.AddTag(
		FGameplayTag::RequestGameplayTag(TEXT("WFC.EdgeType.Door"))
	);
	PathConstraint->PassableEdgeTags.AddTag(
		FGameplayTag::RequestGameplayTag(TEXT("WFC.EdgeType.Corridor"))
	);

	// 设置起点和终点（网格坐标）
	PathConstraint->StartCellLocation = FIntVector(0, 0, 0);  // 左下角
	PathConstraint->EndCellLocation = FIntVector(9, 9, 0);    // 右上角

	// 最小可通过边缘数（2 = 走廊，3+ = 房间）
	PathConstraint->MinPassableEdges = 2;

	// 验证频率（每 N 个单元格选择验证一次）
	PathConstraint->ValidationFrequency = 10;

	// 添加到生成器的约束列表
	WFCGenerator->GetGenerator()->GetConstraints().Add(PathConstraint);

	UE_LOG(LogTemp, Log, TEXT("已添加 2D 迷宫寻路约束"));
}

// ============================================================================
// 示例 2: C++ 中添加寻路约束 - 3D 地下城
// ============================================================================
void SetupMultiFloorDungeon(UWFCGeneratorComponent* WFCGenerator)
{
	if (!WFCGenerator || !WFCGenerator->GetGenerator())
	{
		return;
	}

	UWFCPathfindingConstraint* PathConstraint = NewObject<UWFCPathfindingConstraint>(
		WFCGenerator->GetGenerator()
	);

	PathConstraint->bEnablePathfinding = true;

	// 添加多种可通过类型：门、楼梯、走廊
	PathConstraint->PassableEdgeTags.AddTag(
		FGameplayTag::RequestGameplayTag(TEXT("WFC.EdgeType.Door"))
	);
	PathConstraint->PassableEdgeTags.AddTag(
		FGameplayTag::RequestGameplayTag(TEXT("WFC.EdgeType.Stairs"))
	);
	PathConstraint->PassableEdgeTags.AddTag(
		FGameplayTag::RequestGameplayTag(TEXT("WFC.EdgeType.Corridor"))
	);

	// 起点：地面层入口
	PathConstraint->StartCellLocation = FIntVector(0, 0, 0);

	// 终点：顶层宝藏室
	PathConstraint->EndCellLocation = FIntVector(5, 5, 2);

	PathConstraint->MinPassableEdges = 2;

	// 3D 地下城可以减少验证频率以提升性能
	PathConstraint->ValidationFrequency = 15;

	// 允许更多迭代次数处理复杂路径
	PathConstraint->MaxPathfindingIterations = 20000;

	WFCGenerator->GetGenerator()->GetConstraints().Add(PathConstraint);

	UE_LOG(LogTemp, Log, TEXT("已添加 3D 地下城寻路约束"));
}

// ============================================================================
// 示例 3: 使用 Blueprint 函数库（推荐用于 Blueprint）
// ============================================================================
void SetupUsingBlueprintLibrary(UWFCGeneratorComponent* WFCGenerator)
{
	// 方法 1: 简单迷宫设置
	FGameplayTag DoorTag = FGameplayTag::RequestGameplayTag(TEXT("WFC.EdgeType.Door"));
	
	UWFCPathfindingConstraint* Constraint = UWFCPathfindingBlueprintLibrary::AddSimpleMazePathfinding(
		WFCGenerator,
		FIntVector(0, 0, 0),   // 起点
		FIntVector(10, 10, 0), // 终点
		DoorTag
	);

	// 方法 2: 地下城设置
	FGameplayTag StairsTag = FGameplayTag::RequestGameplayTag(TEXT("WFC.EdgeType.Stairs"));
	FGameplayTag CorridorTag = FGameplayTag::RequestGameplayTag(TEXT("WFC.EdgeType.Corridor"));
	
	Constraint = UWFCPathfindingBlueprintLibrary::AddDungeonPathfinding(
		WFCGenerator,
		FIntVector(0, 0, 0),
		FIntVector(5, 5, 2),
		DoorTag,
		StairsTag,
		CorridorTag
	);
}

// ============================================================================
// 示例 4: 在多人游戏中使用（保持确定性）
// ============================================================================
void SetupMultiplayerMaze(UWFCGeneratorComponent* WFCGenerator, int32 SharedSeed)
{
	// 1. 首先设置多人游戏模式
	WFCGenerator->bUseMultiplayerSeeds = true;
	WFCGenerator->bRequestSeedFromServer = !WFCGenerator->GetOwner()->HasAuthority();

	// 2. 添加寻路约束（在服务器和客户端上都添加）
	UWFCPathfindingConstraint* PathConstraint = NewObject<UWFCPathfindingConstraint>(
		WFCGenerator->GetGenerator()
	);

	PathConstraint->bEnablePathfinding = true;
	PathConstraint->PassableEdgeTags.AddTag(
		FGameplayTag::RequestGameplayTag(TEXT("WFC.EdgeType.Door"))
	);
	PathConstraint->StartCellLocation = FIntVector(0, 0, 0);
	PathConstraint->EndCellLocation = FIntVector(10, 10, 0);
	PathConstraint->MinPassableEdges = 2;

	WFCGenerator->GetGenerator()->GetConstraints().Add(PathConstraint);

	// 3. 使用相同的种子在服务器上启动生成
	if (WFCGenerator->GetOwner()->HasAuthority())
	{
		WFCGenerator->SetSeedAndRun(SharedSeed);
		UE_LOG(LogTemp, Log, TEXT("服务器启动 WFC 生成，种子: %d"), SharedSeed);
	}

	// 结果：所有客户端将生成相同的可解迷宫！
	// 寻路约束不会破坏 RandomStream 的确定性
}

// ============================================================================
// 示例 5: 生成后验证路径
// ============================================================================
void ValidateGeneratedMaze(UWFCGeneratorComponent* WFCGenerator)
{
	FGameplayTagContainer PassableTags;
	PassableTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("WFC.EdgeType.Door")));
	PassableTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("WFC.EdgeType.Corridor")));

	int32 PathLength = 0;
	bool bHasValidPath = UWFCPathfindingBlueprintLibrary::ValidateMazePath(
		WFCGenerator,
		PassableTags,
		FIntVector(0, 0, 0),   // 起点
		FIntVector(10, 10, 0), // 终点
		PathLength
	);

	if (bHasValidPath)
	{
		UE_LOG(LogTemp, Log, TEXT("? 迷宫可解！路径长度: %d"), PathLength);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("? 迷宫不可解！起点和终点之间无路径"));
	}
}

// ============================================================================
// 示例 6: 配置 Tile 资产的边缘标签
// ============================================================================
/*
在你的 Tile 资产（UWFCTileAsset3D 或 UWFCTileAsset2D）中：

1. 创建 GameplayTag 层次结构：
   - WFC.EdgeType
     - WFC.EdgeType.Door      (可通过)
     - WFC.EdgeType.Corridor  (可通过)
     - WFC.EdgeType.Stairs    (可通过)
     - WFC.EdgeType.Wall      (不可通过)
     - WFC.EdgeType.Empty     (不可通过)

2. 为每个 Tile 定义设置 EdgeTypes：

   走廊 Tile (1×1×1):
   - +X: WFC.EdgeType.Door
   - -X: WFC.EdgeType.Door
   - +Y: WFC.EdgeType.Wall
   - -Y: WFC.EdgeType.Wall
   - +Z: WFC.EdgeType.Wall
   - -Z: WFC.EdgeType.Wall
   → 可通过（2 个可通过边缘）

   房间 Tile (3×3×3):
   - +X: WFC.EdgeType.Door
   - -X: WFC.EdgeType.Door
   - +Y: WFC.EdgeType.Door
   - -Y: WFC.EdgeType.Door
   - +Z: WFC.EdgeType.Empty
   - -Z: WFC.EdgeType.Empty
   → 可通过（4 个可通过边缘）

   死胡同 Tile:
   - +X: WFC.EdgeType.Door
   - -X: WFC.EdgeType.Wall
   - +Y: WFC.EdgeType.Wall
   - -Y: WFC.EdgeType.Wall
   - +Z: WFC.EdgeType.Wall
   - -Z: WFC.EdgeType.Wall
   → 不可通过（只有 1 个可通过边缘，默认需要 2 个）
   
   如果要允许死胡同，设置 MinPassableEdges = 1
*/

// ============================================================================
// 重要提示
// ============================================================================
/*
1. **不会破坏 RandomStream 确定性**
   - 寻路约束只是"禁止"某些 Tile，不改变随机选择
   - 使用相同的种子 = 相同的结果
   - 完美支持多人游戏同步

2. **性能优化**
   - ValidationFrequency 控制验证频率
   - 小网格(10×10): 使用 5-10
   - 中等网格(50×50): 使用 20-30
   - 大网格(100×100): 使用 50+

3. **Tile 设计建议**
   - 确保有足够的"可通过" Tile
   - MinPassableEdges = 2 适合大多数迷宫
   - MinPassableEdges = 1 允许死胡同
   - 大房间 Tile 应该有 3+ 个可通过边缘

4. **调试技巧**
   - 使用 ValidateMazePath 在生成后验证
   - 检查日志中的 "PathfindingConstraint" 消息
   - 如果出现矛盾，增加 Tile 种类或降低 MinPassableEdges

5. **Blueprint 使用**
   - 使用 UWFCPathfindingBlueprintLibrary 中的辅助函数
   - AddSimpleMazePathfinding 适合简单迷宫
   - AddDungeonPathfinding 适合复杂地下城
   - ValidateMazePath 可以在完成后验证
*/
