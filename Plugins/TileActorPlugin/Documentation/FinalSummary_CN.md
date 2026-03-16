# WFC 寻路约束 - 最终实现总结

## ? 已完成功能

我已经成功实现了一个**完全"傻瓜式"操作**的 WFC 寻路验证约束系统！

### ?? 核心功能

1. **? A* 寻路验证**：确保起点到终点存在有效路径
2. **? 编辑器中配置**：无需写代码，直接在 WFC Asset 中添加和配置
3. **? 保持 RandomStream 确定性**：完美支持多人游戏网络复制
4. **? 支持 2D/3D**：自动适配不同类型的网格
5. **? Blueprint 辅助函数**：提供快速设置函数（可选）
6. **? 性能可控**：通过参数调节验证频率

---

## ?? 创建的文件

### 1. 核心代码文件

| 文件 | 说明 |
|------|------|
| `WFCPathfindingConstraint.h` | 约束类头文件 |
| `WFCPathfindingConstraint.cpp` | 约束类实现 |
| `WFCPathfindingBlueprintLibrary.h` | Blueprint 辅助库头文件 |
| `WFCPathfindingBlueprintLibrary.cpp` | Blueprint 辅助库实现 |

### 2. 文档文件

| 文件 | 说明 |
|------|------|
| `PathfindingConstraint.md` | 英文完整 API 文档 |
| `PathfindingImplementationSummary_CN.md` | 中文技术总结 |
| `PathfindingUsageExamples.cpp` | 代码使用示例（6个）|
| `PathfindingConstraint_EditorGuide_CN.md` | **编辑器使用指南（傻瓜式）** ? |

---

## ?? 如何使用（傻瓜式3步）

### 第1步：在 WFC Asset 中添加约束

1. 打开你的 **WFC Asset** 数据资产
2. 找到 **Config** → **Constraints** 数组
3. 点击 **+** 添加新约束
4. 选择：**WFCPathfindingConstraint (Pathfinding Constraint - Maze Solvability)**

### 第2步：配置约束参数

在约束编辑器中设置：

```
? Enable Pathfinding: 勾选

?? Passable Edge Tags: 
   - WFC.EdgeType.Door
   - WFC.EdgeType.Corridor

?? Start Cell Location: (0, 0, 0)
?? End Cell Location: (9, 9, 0)

?? Min Passable Edges: 2
?? Validation Frequency: 10
```

### 第3步：配置 Tile 边缘标签

在你的 Tile 资产（UWFCTileAsset3D/2D）中：

1. 创建 GameplayTag：`WFC.EdgeType.Door`（可通过）
2. 为每个 Tile 设置 **Edge Types**
3. 确保走廊类 Tile 有至少 2 个"Door"边缘

### 完成！

运行游戏，生成的迷宫将自动保证可解！??

---

## ?? 在哪里配置

### ? 推荐方式：WFC Asset 中配置

**位置**：
```
WFC Asset（数据资产）
  └─ Config
      └─ Constraints（约束数组）
          └─ + 添加 WFCPathfindingConstraint
              ├─ Enable Pathfinding ?
              ├─ Passable Edge Tags
              ├─ Start Cell Location
              ├─ End Cell Location
              ├─ Min Passable Edges: 2
              └─ Validation Frequency: 10
```

**优点**：
- ? 最简单，无需写代码
- ? 所有使用该 Asset 的 Actor 自动应用
- ? 可视化配置，直观清晰
- ? 保存在资产中，可复用

### 备选方式：Blueprint 中动态添加

使用 **WFCPathfindingBlueprintLibrary** 提供的函数：

**节点**：
- `Add Simple Maze Pathfinding`（快速设置简单迷宫）
- `Add Dungeon Pathfinding`（快速设置地下城）
- `Add Pathfinding Constraint`（通用设置）

**使用场景**：需要运行时动态修改约束参数

---

## ?? 关键特性

### 1. 不影响 RandomStream

```
WFC 正常选择 Tile（使用 RandomStream）
    ↓
约束检查路径是否存在
    ↓
如果路径无效，禁止不可通过的 Tile
    ↓
WFC 从剩余 Tile 继续选择（仍使用 RandomStream）
```

**结果**：
- ? 相同种子 = 相同的可解迷宫
- ? 多人游戏完美同步
- ? 超低带宽（只需复制种子）

### 2. Tile 可通过性判断

一个 Tile 可通过，如果：
```cpp
可通过边缘数量 >= MinPassableEdges
```

**示例**：
- 走廊（2 个门）：可通过 ?
- 房间（4 个门）：可通过 ?
- 死胡同（1 个门）：不可通过 ?（除非 MinPassableEdges = 1）

### 3. A* 寻路算法

- **启发函数**：曼哈顿距离
- **代价函数**：统一代价（每步 = 1.0）
- **性能优化**：可调节验证频率

---

## ?? 配置参数说明

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| **Enable Pathfinding** | bool | false | 启用约束 |
| **Passable Edge Tags** | TagContainer | 空 | 可通过的边缘标签 |
| **Start Cell Location** | IntVector | (0,0,0) | 起点网格坐标 |
| **End Cell Location** | IntVector | (1,1,1) | 终点网格坐标 |
| **Min Passable Edges** | int | 2 | 最小可通过边缘数 |
| **Validation Frequency** | int | 10 | 每N个单元格验证一次 |
| **Max Pathfinding Iterations** | int | 10000 | 最大A*迭代次数 |

---

## ?? 使用场景

### 场景1：2D 迷宫（10×10）
```
Passable Tags: Door
Start: (0, 0, 0)
End: (9, 9, 0)
Min Edges: 2
Frequency: 10
```

### 场景2：大型地图（50×50）
```
Passable Tags: Door, Corridor
Start: (0, 0, 0)
End: (49, 49, 0)
Min Edges: 2
Frequency: 30  ← 提高频率以优化性能
```

### 场景3：3D 地下城（10×10×3）
```
Passable Tags: Door, Stairs, Corridor
Start: (0, 0, 0)  ← 地面层入口
End: (9, 9, 2)    ← 顶层宝藏室
Min Edges: 2
Frequency: 15
```

---

## ?? 调试技巧

### 1. 查看日志

运行游戏后查看输出日志：

```
LogWFC: PathfindingConstraint initialized: Start=(0,0,0), End=(9,9,0), PassableTags=2
LogWFC: PathfindingConstraint: Valid path found with 15 cells
```

### 2. 验证路径

使用 Blueprint 函数验证：

```
Validate Maze Path
  ├─ Generator Component
  ├─ Passable Tags
  ├─ Start Location
  ├─ End Location
  └─ Out Path Length → 显示路径长度
```

### 3. 测试多个种子

```cpp
// 测试多次生成
for (int32 i = 0; i < 10; i++)
{
    WFCGenerator->SetSeedAndRun(12345 + i);
    // 每次都应该生成可解迷宫
}
```

---

## ?? 常见问题

### Q1：约束不起作用？
**检查**：
- ? 在 WFC Asset 的 Constraints 中添加了
- ? Enable Pathfinding 已勾选
- ? Passable Edge Tags 不为空

### Q2：找不到有效路径？
**解决**：
- 检查 Tile 边缘标签是否正确
- 降低 Min Passable Edges
- 增加可通过类型的 Tile

### Q3：生成速度慢？
**优化**：
- 增加 Validation Frequency（30-50）
- 确保有足够的可通过 Tile 种类

### Q4：多人游戏不同步？
**检查**：
- 所有客户端使用相同种子
- 约束参数在所有客户端相同
- 启用 bUseMultiplayerSeeds

---

## ?? 性能建议

| 网格大小 | Validation Frequency | 说明 |
|----------|---------------------|------|
| 10×10 | 5-10 | 小网格，可频繁验证 |
| 50×50 | 20-30 | 中等网格，平衡性能 |
| 100×100 | 50+ | 大网格，减少验证 |
| 3D 地下城 | 15-20 | 更复杂，稍低频率 |

---

## ?? 总结

### 实现的目标

1. ? **完全傻瓜式操作**：在编辑器中可视化配置，无需写代码
2. ? **保证迷宫可解**：A* 寻路验证起点到终点有通路
3. ? **保持确定性**：不影响 RandomStream，完美支持多人游戏
4. ? **灵活配置**：支持 2D/3D，可自定义可通过性规则
5. ? **性能可控**：可调节验证频率平衡质量和性能

### 使用流程

```
在 WFC Asset 中添加约束
    ↓
配置参数（起点、终点、可通过标签等）
    ↓
配置 Tile 边缘标签
    ↓
运行游戏 → 自动生成可解迷宫！
```

### 无需任何代码！

所有配置都在编辑器中完成，是真正的"傻瓜式"操作！

---

## ?? 文档位置

- **编辑器使用指南**：`PathfindingConstraint_EditorGuide_CN.md` ?
- **技术总结**：`PathfindingImplementationSummary_CN.md`
- **代码示例**：`PathfindingUsageExamples.cpp`
- **完整文档**：`PathfindingConstraint.md`

---

祝你使用愉快！??

如果有任何问题，请查看 **编辑器使用指南** 获取详细的图文教程。
