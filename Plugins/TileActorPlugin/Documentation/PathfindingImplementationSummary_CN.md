# WFC 寻路验证功能 - 实现总结

## 功能概述

我已经成功为你的 WFC 插件实现了 A* 寻路验证功能，确保生成的迷宫/地下城是可解的（起点和终点之间存在通路）。

### ? 核心特性

1. **保证迷宫可解**：通过 A* 寻路算法验证起点到终点存在有效路径
2. **不影响 RandomStream**：约束只禁止会阻塞路径的 Tile，不改变随机选择逻辑
3. **完美支持网络复制**：确定性生成，所有客户端使用相同种子产生相同结果
4. **支持 2D/3D**：自动适配 `UWFCGrid2D` 和 `UWFCGrid3D`
5. **灵活的可通过性定义**：通过 GameplayTag 系统定义哪些边缘可以通过
6. **性能可控**：通过验证频率参数平衡准确性和性能

## 已创建的文件

### 1. 核心约束类
- **头文件**：`Plugins/WFC/Source/WFC/Public/Core/Constraints/WFCPathfindingConstraint.h`
- **实现文件**：`Plugins/WFC/Source/WFC/Private/Core/Constraints/WFCPathfindingConstraint.cpp`

**主要功能**：
- `UWFCPathfindingConstraint` 类：实现 A* 寻路验证
- `FWFCPathNode` 结构：A* 算法的节点数据
- `ValidatePath()`：核心 A* 寻路实现
- `IsTileTraversable()`：判断 Tile 是否可通过
- `BanBlockingTiles()`：禁止会阻塞路径的 Tile

### 2. Blueprint 辅助库
- **头文件**：`Plugins/WFC/Source/WFC/Public/WFCPathfindingBlueprintLibrary.h`
- **实现文件**：`Plugins/WFC/Source/WFC/Private/WFCPathfindingBlueprintLibrary.cpp`

**提供的 Blueprint 函数**：
- `AddPathfindingConstraint()`：通用添加函数
- `AddSimpleMazePathfinding()`：快速设置简单迷宫
- `AddDungeonPathfinding()`：快速设置多层地下城
- `ValidateMazePath()`：验证生成后的路径

### 3. 文档
- **功能说明**：`Plugins/WFC/Documentation/PathfindingConstraint.md`（英文详细文档）
- **使用示例**：`Plugins/WFC/Documentation/PathfindingUsageExamples.cpp`（中文代码示例）

## 工作原理

### 约束执行流程

```
WFC 生成开始
    ↓
初始化约束（计算起点/终点单元格索引）
    ↓
每次选择单元格后
    ↓
CellSelectionCounter++
    ↓
达到 ValidationFrequency?
    ↓ 是
运行 A* 寻路验证
    ↓
找到有效路径？
    ↓ 否
禁止关键单元格中的不可通过 Tile
    ↓
继续 WFC 生成
```

### Tile 可通过性判断

一个 Tile 被认为是"可通过的"需要满足：

```cpp
int32 PassableEdgeCount = 0;
for (每个边缘方向) {
    if (边缘标签在 PassableEdgeTags 中) {
        PassableEdgeCount++;
    }
}
return PassableEdgeCount >= MinPassableEdges;
```

**典型配置**：
- `MinPassableEdges = 2`：走廊（至少 2 个对向的门）
- `MinPassableEdges = 1`：允许死胡同
- `MinPassableEdges = 3`：房间或交叉口（多个出口）

### A* 寻路算法

使用标准 A* 实现：
- **启发函数**：曼哈顿距离（Manhattan distance）
- **代价函数**：统一代价（每步 = 1.0）
- **开放列表**：按 F-cost 排序的数组
- **关闭列表**：使用 TSet 实现 O(1) 查找

## 如何使用

### C++ 快速开始

```cpp
#include "Core/Constraints/WFCPathfindingConstraint.h"

void SetupMaze(UWFCGeneratorComponent* Generator)
{
    // 创建约束
    UWFCPathfindingConstraint* Constraint = 
        NewObject<UWFCPathfindingConstraint>(Generator->GetGenerator());
    
    // 配置
    Constraint->bEnablePathfinding = true;
    Constraint->PassableEdgeTags.AddTag(
        FGameplayTag::RequestGameplayTag(TEXT("WFC.EdgeType.Door"))
    );
    Constraint->StartCellLocation = FIntVector(0, 0, 0);
    Constraint->EndCellLocation = FIntVector(9, 9, 0);
    Constraint->MinPassableEdges = 2;
    
    // 添加到生成器
    Generator->GetGenerator()->GetConstraints().Add(Constraint);
}
```

### Blueprint 快速开始

在 Blueprint 中调用：
```
Add Simple Maze Pathfinding
  ├─ Generator Component: WFCGenerator
  ├─ Start Location: (0, 0, 0)
  ├─ End Location: (10, 10, 0)
  └─ Door Tag: WFC.EdgeType.Door
```

### 多人游戏使用

```cpp
// 服务器和客户端都添加相同的约束
SetupMaze(WFCGenerator);

// 服务器启动生成
if (HasAuthority()) {
    WFCGenerator->SetSeedAndRun(12345);
}

// 结果：所有客户端生成相同的可解迷宫！
```

## 配置 Tile 资产

### 创建 GameplayTag 层次

在项目设置中创建：
```
WFC.EdgeType
  ├─ WFC.EdgeType.Door      (可通过)
  ├─ WFC.EdgeType.Corridor  (可通过)
  ├─ WFC.EdgeType.Stairs    (可通过，用于 3D)
  ├─ WFC.EdgeType.Wall      (不可通过)
  └─ WFC.EdgeType.Empty     (不可通过)
```

### 配置 Tile 边缘

**走廊 Tile (1×1×1)**：
```
EdgeTypes:
  +X: Door
  -X: Door
  其他: Wall
→ 可通过（2 个门）
```

**房间 Tile (3×3×3)**：
```
EdgeTypes:
  +X: Door
  -X: Door
  +Y: Door
  -Y: Door
  其他: Wall
→ 可通过（4 个门）
```

**死胡同 Tile**：
```
EdgeTypes:
  +X: Door
  其他: Wall
→ 不可通过（只有 1 个门，默认需要 2 个）
```

## 性能优化建议

### ValidationFrequency 设置

| 网格大小 | 推荐值 | 说明 |
|---------|--------|------|
| 10×10×1 | 5-10   | 频繁验证，确保质量 |
| 50×50×1 | 20-30  | 平衡性能和质量 |
| 100×100×1 | 50+  | 减少性能影响 |
| 10×10×10 (3D) | 15-20 | 3D 更复杂，稍微降低频率 |

### 其他优化

```cpp
// 对于大型网格
Constraint->ValidationFrequency = 50;
Constraint->MaxPathfindingIterations = 20000;

// 对于小型网格（高质量）
Constraint->ValidationFrequency = 5;
Constraint->MaxPathfindingIterations = 5000;
```

## 与 RandomStream 的兼容性

### ? 完全兼容！

寻路约束的工作方式：

1. **WFC 正常选择 Tile**（使用 RandomStream）
2. **约束检查路径**
3. **如果路径无效，约束"禁止"某些 Tile**
4. **WFC 从剩余 Tile 中继续选择**（仍使用 RandomStream）

### 关键点

- ? **约束不会调用** `RandomStream.GetFraction()` 或类似方法
- ? **约束只调用** `Generator->Ban()` 和 `Generator->BanMultiple()`
- ? **RandomStream 的确定性完全保持**
- ? **相同种子 = 相同的可解迷宫**

### 网络复制测试

```cpp
// 测试确定性
void TestDeterminism()
{
    const int32 Seed = 12345;
    
    // 运行 10 次
    for (int i = 0; i < 10; i++) {
        WFCGenerator->SetSeedAndRun(Seed);
        // 每次都产生相同的结果！
    }
}
```

## 调试和故障排除

### 启用详细日志

```cpp
// 在代码中
UE_LOG(LogWFC, Verbose, TEXT("..."));

// 或在 DefaultEngine.ini 中
[Core.Log]
LogWFC=Verbose
```

### 常见问题

**问题 1：找不到有效路径**
- 检查 `PassableEdgeTags` 是否包含所有可通过标签
- 验证 Tile 资产的边缘标签设置正确
- 降低 `MinPassableEdges`
- 增加可通过 Tile 的数量

**问题 2：生成过程中出现矛盾**
- 增加 Tile 种类
- 降低 `MinPassableEdges`
- 增加 `ValidationFrequency`（减少验证次数）
- 检查其他约束是否过于严格

**问题 3：多人游戏中结果不一致**
- 确保所有客户端使用相同的种子
- 验证约束参数在所有客户端上相同
- 确保 Tile 资产和标签完全相同
- 启用 `bUseMultiplayerSeeds`

## 未来增强可能性

基于当前实现，未来可以添加：

1. **多起点/终点对**：支持多个必须连通的区域
2. **路径质量指标**：偏好更短或更长的路径
3. **单向通道支持**：某些门只能单向通过
4. **自定义启发函数**：不同类型的路径寻找
5. **可视化调试**：在编辑器中显示路径
6. **路径缓存**：优化性能
7. **动态重新验证**：在运行时重新检查路径

## 技术细节

### 约束类继承层次

```
UObject
  └─ UWFCConstraint (基类)
      └─ UWFCPathfindingConstraint (新增)
```

### 主要数据结构

```cpp
struct FWFCPathNode {
    FWFCCellIndex CellIndex;  // 单元格索引
    FWFCCellIndex ParentIndex; // 父节点索引
    float GCost;  // 从起点的代价
    float HCost;  // 到终点的启发代价
    float FCost;  // 总代价 (G + H)
};
```

### 统计和性能监控

使用 Unreal 的 Stats 系统：
```cpp
STAT_WFCPathfindingValidation    // 验证时间
STAT_WFCPathfindingChecks        // 验证次数
STAT_WFCPathfindingIterations    // A* 迭代次数
```

在控制台中查看：
```
stat WFC
```

## 总结

### 实现的功能

? A* 寻路验证约束  
? 2D/3D 网格支持  
? GameplayTag 系统集成  
? Blueprint 辅助函数  
? RandomStream 兼容（确定性保持）  
? 多人游戏支持  
? 性能可配置  
? 完整文档和示例  

### 使用建议

1. **简单 2D 迷宫**：使用 `AddSimpleMazePathfinding()`
2. **复杂 3D 地下城**：使用 `AddDungeonPathfinding()`
3. **自定义需求**：直接创建 `UWFCPathfindingConstraint` 并配置
4. **多人游戏**：确保所有客户端添加相同的约束

### 注意事项

- 确保 Tile 资产有正确的边缘标签
- 根据网格大小调整 `ValidationFrequency`
- 提供足够的可通过 Tile 种类
- 在测试环境中验证路径生成

## 联系和支持

如果遇到问题或需要更多功能：
1. 检查 `PathfindingConstraint.md` 详细文档
2. 查看 `PathfindingUsageExamples.cpp` 代码示例
3. 启用详细日志进行调试

祝你使用愉快！??
