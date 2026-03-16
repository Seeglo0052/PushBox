# WFC 寻路约束 - 编辑器使用指南（傻瓜式操作）

## ?? 快速开始 - 3步配置

### 第1步：创建或打开 WFC Asset

1. 在内容浏览器中，找到你的 **WFC Asset**（数据资产）
2. 双击打开它

### 第2步：添加寻路约束

1. 在 WFC Asset 编辑器中，找到 **Config** 分类
2. 找到 **Constraints** 数组
3. 点击 **+** 添加新约束
4. 在下拉列表中选择：**WFCPathfindingConstraint (Pathfinding Constraint - Maze Solvability)**

![添加约束](添加约束.png)

### 第3步：配置约束参数

点击新添加的约束旁边的 **编辑** 按钮，配置以下参数：

#### 基础设置（必填）

| 参数名 | 说明 | 示例值 |
|--------|------|--------|
| **Enable Pathfinding** | 启用寻路验证 | ? 勾选 |
| **Passable Edge Tags** | 可通过的边缘标签 | WFC.EdgeType.Door<br>WFC.EdgeType.Corridor |
| **Start Cell Location** | 起点位置（网格坐标） | (0, 0, 0) |
| **End Cell Location** | 终点位置（网格坐标） | (9, 9, 0) |
| **Min Passable Edges** | 最小可通过边缘数 | 2 |

#### 高级设置（可选）

| 参数名 | 说明 | 推荐值 |
|--------|------|--------|
| **Validation Frequency** | 验证频率 | 10 |
| **Max Pathfinding Iterations** | 最大迭代次数 | 10000 |

![配置约束](配置约束.png)

### 完成！

保存 WFC Asset，然后运行游戏。生成的迷宫将自动保证起点和终点之间有通路！

---

## ?? 详细配置说明

### 1. Enable Pathfinding（启用寻路）
- **类型**：布尔值（勾选框）
- **说明**：勾选后启用寻路验证功能
- **默认值**：false（未勾选）
- **建议**：? 勾选

### 2. Passable Edge Tags（可通过边缘标签）
- **类型**：GameplayTag 容器
- **说明**：定义哪些边缘标签表示可以通过
- **如何设置**：
  1. 点击 **+** 添加标签
  2. 从列表中选择或创建标签
  3. 常用标签：
     - `WFC.EdgeType.Door`（门）
     - `WFC.EdgeType.Corridor`（走廊）
     - `WFC.EdgeType.Stairs`（楼梯，3D 地下城）

![添加标签](添加标签.png)

### 3. Start Cell Location（起点位置）
- **类型**：FIntVector（X, Y, Z）
- **说明**：迷宫起点的网格坐标
- **注意**：
  - 坐标从 (0, 0, 0) 开始
  - 必须在网格范围内
  - 例如：10×10 的网格，有效范围是 (0-9, 0-9, 0)
- **示例**：
  - 2D 迷宫左下角：`(0, 0, 0)`
  - 3D 地下城入口：`(0, 0, 0)`

### 4. End Cell Location（终点位置）
- **类型**：FIntVector（X, Y, Z）
- **说明**：迷宫终点的网格坐标
- **示例**：
  - 2D 迷宫右上角：`(9, 9, 0)`
  - 3D 地下城顶层：`(5, 5, 2)`

### 5. Min Passable Edges（最小可通过边缘数）
- **类型**：整数（1-6）
- **说明**：一个 Tile 需要有多少个可通过边缘才被认为是可通过的
- **推荐值**：
  - `1`：允许死胡同
  - `2`：走廊（最常用） ?
  - `3+`：房间或交叉口
- **默认值**：2

### 6. Validation Frequency（验证频率）
- **类型**：整数（1-100）
- **说明**：每选择 N 个单元格后验证一次路径
- **性能影响**：
  - 较小值（1-5）：更频繁验证，更慢
  - 较大值（50+）：较少验证，更快
- **推荐值**：
  - 小网格（10×10）：5-10
  - 中网格（50×50）：20-30 ?
  - 大网格（100×100）：50+
- **默认值**：10

### 7. Max Pathfinding Iterations（最大寻路迭代次数）
- **类型**：整数（100-100000）
- **说明**：A* 算法的最大迭代次数，防止死循环
- **推荐值**：
  - 小网格：5000
  - 中等网格：10000 ?
  - 大网格：20000+
- **默认值**：10000

---

## ?? 配置 Tile 资产的边缘标签

### 创建 GameplayTag

1. **打开项目设置**：编辑 → 项目设置
2. **找到 GameplayTags**：Project → GameplayTags
3. **添加新标签**：

```
WFC.EdgeType
  ├─ WFC.EdgeType.Door      (门，可通过)
  ├─ WFC.EdgeType.Corridor  (走廊，可通过)
  ├─ WFC.EdgeType.Stairs    (楼梯，可通过，用于 3D)
  ├─ WFC.EdgeType.Road      (道路，可通过)
  ├─ WFC.EdgeType.Wall      (墙，不可通过)
  └─ WFC.EdgeType.Empty     (空，不可通过)
```

### 为 Tile 资产设置边缘标签

1. **打开 Tile 资产**（UWFCTileAsset3D 或 UWFCTileAsset2D）
2. **找到每个 Tile Definition**
3. **设置 Edge Types（边缘类型）**

### ? 重要：大型 Tile 和空标签

**对于大型 Tile（如 1×1×2 的斜坡）**：
- **内部边缘可以留空（不设置任何标签）**
- **空标签会被自动视为可通过**
- 这表示大型 Tile 内部是一个整体，内部是连通的

**示例**：1×1×2 斜坡 Tile
```
底层 (Z=0):
  +X: WFC.EdgeType.Wall
  -X: WFC.EdgeType.Wall
  +Y: WFC.EdgeType.Wall
  -Y: WFC.EdgeType.Wall
  +Z: <Empty>              ← 空标签，指向顶层（内部）
  -Z: WFC.EdgeType.Road

顶层 (Z=1):
  +X: WFC.EdgeType.Wall
  -X: WFC.EdgeType.Wall
  +Y: WFC.EdgeType.Wall
  -Y: WFC.EdgeType.Wall
  +Z: WFC.EdgeType.Road
  -Z: <Empty>              ← 空标签，指向底层（内部）
```
→ **结果**：斜坡内部自动连通，可以从底层走到顶层

### 示例 3：死胡同 Tile

```
Edge Types:
  +X: WFC.EdgeType.Door     ? 可通过
  -X: WFC.EdgeType.Wall     ? 不可通过
  +Y: WFC.EdgeType.Wall     ? 不可通过
  -Y: WFC.EdgeType.Wall     ? 不可通过
  +Z: WFC.EdgeType.Wall     ? 不可通过
  -Z: WFC.EdgeType.Wall     ? 不可通过
```
→ **结果**：不可通过（只有 1 个可通过边缘，默认需要 2 个）
→ **如何允许**：设置 `Min Passable Edges = 1`

#### 示例 4：大型斜坡 Tile (1×1×2) ?

```
底层单元格:
  +X: WFC.EdgeType.Wall     ? 不可通过
  -X: WFC.EdgeType.Wall     ? 不可通过
  +Y: WFC.EdgeType.Road     ? 可通过（入口）
  -Y: WFC.EdgeType.Wall     ? 不可通过
  +Z: <Empty>               ? 可通过（内部，自动）
  -Z: WFC.EdgeType.Pillar   ? 不可通过

顶层单元格:
  +X: WFC.EdgeType.Wall     ? 不可通过
  -X: WFC.EdgeType.Wall     ? 不可通过
  +Y: WFC.EdgeType.Wall     ? 不可通过
  -Y: WFC.EdgeType.Road     ? 可通过（出口）
  +Z: WFC.EdgeType.Open     ? 可通过（可能）
  -Z: <Empty>               ? 可通过（内部，自动）
```
→ **结果**：
- 底层有 2 个可通过边缘（+Y 和 +Z）
- 顶层有 2-3 个可通过边缘（-Y, -Z, 可能还有 +Z）
- **内部自动连通**，可以从底层走到顶层

#### 示例 5：大型房间 Tile (3×3×1)

```
中心单元格的边缘标签都是 <Empty>（内部边缘）：
  +X: <Empty>  ? 可通过（内部）
  -X: <Empty>  ? 可通过（内部）
  +Y: <Empty>  ? 可通过（内部）
  -Y: <Empty>  ? 可通过（内部）
  +Z: <Empty>  ? 可通过（内部）
  -Z: <Empty>  ? 可通过（内部）
```
→ **结果**：中心单元格有 6 个可通过边缘（全部内部边缘）

---

## ?? 在 WFCTestingActor 中使用

### 方法1：直接在 WFC Asset 中配置（推荐）?

1. 打开 WFC Asset
2. 在 **Constraints** 中添加 **WFCPathfindingConstraint**
3. 配置参数
4. 保存
5. 运行游戏

**这是最简单的方法！** 约束会自动应用到所有使用该 WFC Asset 的 Actor。

### 方法2：在 Blueprint 中动态添加

1. 打开 WFCTestingActor 的 Blueprint
2. 在 **BeginPlay** 事件后添加节点：

```
BeginPlay
    ↓
Add Simple Maze Pathfinding (来自 WFCPathfindingBlueprintLibrary)
    ├─ Generator Component: WFCGenerator
    ├─ Start Location: (0, 0, 0)
    ├─ End Location: (10, 10, 0)
    └─ Door Tag: WFC.EdgeType.Door
```

### 方法3：在 C++ 中添加

```cpp
void AWFCTestingActor::BeginPlay()
{
    Super::BeginPlay();
    
    // 使用 Blueprint 库快速设置
    FGameplayTag DoorTag = FGameplayTag::RequestGameplayTag(TEXT("WFC.EdgeType.Door"));
    UWFCPathfindingBlueprintLibrary::AddSimpleMazePathfinding(
        WFCGenerator,
        FIntVector(0, 0, 0),
        FIntVector(9, 9, 0),
        DoorTag
    );
}
```

---

## ? 验证配置是否正确

### 1. 检查日志

运行游戏后，查看输出日志（Output Log），应该看到：

```
LogWFC: PathfindingConstraint initialized: Start=(0,0,0), End=(9,9,0), PassableTags=2
LogWFC: PathfindingConstraint: Valid path found with 15 cells
```

### 2. 测试不同种子

在游戏中多次运行生成，确保每次都能生成可解的迷宫：

```
// 在 Blueprint 或 C++ 中
WFCGenerator->SetSeedAndRun(12345);
```

### 3. 使用验证函数

```cpp
// 生成完成后验证
int32 PathLength;
bool bHasPath = UWFCPathfindingBlueprintLibrary::ValidateMazePath(
    WFCGenerator,
    PassableTags,
    StartLocation,
    EndLocation,
    PathLength
);

if (bHasPath)
{
    UE_LOG(LogTemp, Log, TEXT("? Maze is solvable! Path length: %d"), PathLength);
}
else
{
    UE_LOG(LogTemp, Warning, TEXT("? Maze is NOT solvable!"));
}
```

---

## ?? 常见问题排查

### 问题1：约束不起作用

**检查清单**：
- ? 在 WFC Asset 的 **Constraints** 数组中添加了约束
- ? **Enable Pathfinding** 已勾选
- ? **Passable Edge Tags** 不为空
- ? 起点和终点坐标在网格范围内

### 问题2：找不到有效路径

**使用可视化工具**（推荐）：

在 Blueprint 中添加 **Draw Pathfinding Debug** 节点：

```
Run New Generation
    ↓
Delay (1 second)
    ↓
Draw Pathfinding Debug  ← 可视化工具
    ├─ Generator Component: WFCGenerator
    ├─ Passable Edge Tags: [你的标签]
    ├─ Start Location: (0, 0, 0)
    ├─ End Location: (9, 9, 1)
    └─ Duration: 10.0
```

**显示内容**：
- ?? **绿色球** = 起点
- ?? **红色球** = 终点
- ?? **蓝色线 + 青色球** = 找到的路径
- ?? **黄色球** = 从起点可到达的区域
- ? **红色X** = 找不到路径

**详细说明请查看**：`PathfindingVisualization_CN.md`

---

**或使用诊断工具**：

在 Blueprint 中添加 **Diagnose Pathfinding Issue** 节点：

```
Run New Generation
    ↓
Diagnose Pathfinding Issue  ← 诊断工具
    ├─ Generator Component: WFCGenerator
    ├─ Passable Edge Tags: [你的标签]
    ├─ Start Location: (0, 0, 0)
    ├─ End Location: (9, 9, 0)
    └─ Min Passable Edges: 2
    ↓
Validate Maze Path
```

然后查看 **Output Log**，会显示详细的诊断信息：
- 哪个单元格不可通过
- 为什么找不到路径
- 如何修复问题

**详细说明请查看**：`PathfindingDiagnosisGuide_CN.md`

**常见原因**：
1. 检查 **Passable Edge Tags** 是否包含所有可通过标签
2. 验证 Tile 资产的边缘标签设置正确
3. 降低 **Min Passable Edges**（例如从 2 改为 1）
4. 增加可通过类型的 Tile 数量

### 问题3：生成速度很慢

**优化方案**：
1. 增加 **Validation Frequency**（例如从 10 改为 30）
2. 对于大网格，使用 50 或更高
3. 确保有足够的可通过 Tile 种类

### 问题4：生成过程中出现矛盾

**解决方案**：
1. 增加 Tile 种类和数量
2. 降低 **Min Passable Edges**
3. 增加 **Validation Frequency**（减少验证次数）
4. 检查其他约束是否过于严格

### 问题5：大型 Tile 内部无法通过

**症状**：
- 使用了大型 Tile（如 1×1×2 斜坡）
- 诊断显示内部边缘不可通过

**解决方案**：
? **无需配置！** 系统自动处理：
- 空标签（未设置的边缘标签）自动视为可通过
- 这表示大型 Tile 内部是连通的
- 不需要添加任何特殊标签到 `Passable Edge Tags`

**验证方法**：
查看诊断日志，空标签显示为 `<Empty/Interior> [OK]`：
```
LogWFC: +Z: <Empty/Interior> [OK]  ← 大型 Tile 内部，自动可通过
LogWFC: -Z: <Empty/Interior> [OK]
```

### 问题6：跨层路径找不到

**症状**：
- 起点在 Z=0，终点在 Z=1
- 诊断显示起点和终点都正常
- 但找不到路径

**原因**：
可能缺少连接不同层的 Tile（如斜坡、楼梯）

**解决方案**：
1. **检查是否有跨层 Tile**：
   - 确保有 1×1×2 或更大的 Tile
   - 这些 Tile 的内部边缘应该是空的

2. **检查跨层 Tile 的外部边缘**：
   - 底层入口：应该有可通过标签（如 `WFC.EdgeType.Road`）
   - 顶层出口：应该有可通过标签
   - 内部连接：留空即可（自动可通过）

3. **确保有足够的跨层 Tile**：
   - 增加斜坡/楼梯 Tile 的数量
   - 或者降低其他约束的限制

---

## ?? 配置模板

### 模板1：简单 2D 迷宫（10×10）

```
Enable Pathfinding: ?
Passable Edge Tags: 
  - WFC.EdgeType.Door
Start Cell Location: (0, 0, 0)
End Cell Location: (9, 9, 0)
Min Passable Edges: 2
Validation Frequency: 10
Max Pathfinding Iterations: 10000
```

### 模板2：大型 2D 迷宫（50×50）

```
Enable Pathfinding: ?
Passable Edge Tags: 
  - WFC.EdgeType.Door
  - WFC.EdgeType.Corridor
Start Cell Location: (0, 0, 0)
End Cell Location: (49, 49, 0)
Min Passable Edges: 2
Validation Frequency: 30
Max Pathfinding Iterations: 20000
```

### 模板3：3D 地下城（10×10×3）

```
Enable Pathfinding: ?
Passable Edge Tags: 
  - WFC.EdgeType.Door
  - WFC.EdgeType.Stairs
  - WFC.EdgeType.Corridor
Start Cell Location: (0, 0, 0)
End Cell Location: (9, 9, 2)
Min Passable Edges: 2
Validation Frequency: 15
Max Pathfinding Iterations: 15000
```

---

## ?? 总结

### 最简单的3步流程：

1. **在 WFC Asset 中添加约束**
   - Constraints → + → WFCPathfindingConstraint

2. **配置必填参数**
   - Enable Pathfinding ?
   - Passable Edge Tags（添加可通过标签）
   - Start/End Cell Location（设置起点终点）

3. **配置 Tile 边缘标签**
   - 在 Tile 资产中设置 Edge Types
   - 确保有足够的可通过 Tile

### 就这么简单！??

不需要写任何代码，不需要在 Blueprint 中添加节点。约束会自动应用，确保生成的迷宫可解！

---

## ?? 提示

- **保存 WFC Asset 后**，所有使用该 Asset 的 Actor 都会应用约束
- **多人游戏兼容**：约束不影响 RandomStream，确保所有客户端生成相同结果
- **性能提示**：根据网格大小调整 Validation Frequency
- **调试技巧**：启用日志查看寻路验证过程

祝你使用愉快！??
