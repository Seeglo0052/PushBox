# 寻路诊断工具使用指南

## 问题：找不到路径

如果你的 `Validate Maze Path` 节点返回 `false`，说明找不到从起点到终点的路径。

## 解决方案：使用诊断工具

我创建了一个诊断工具，可以帮你找出问题所在。

### 在 Blueprint 中使用

在你的 Blueprint 中，在 `Run New Generation` 和 `Validate Maze Path` 之间添加这个节点：

```
Run New Generation
    ↓
Diagnose Pathfinding Issue  ← 添加这个节点
    ├─ Generator Component: Self → WFC Generator
    ├─ Passable Edge Tags: [你的可通过标签]
    ├─ Start Location: (0, 0, 0)
    ├─ End Location: (9, 9, 0)
    └─ Min Passable Edges: 2
    ↓
Validate Maze Path
```

### 查看诊断结果

运行游戏后，打开 **Output Log**（输出日志），搜索 `PathFinding`，你会看到详细的诊断信息：

```
========================================
=== Pathfinding Diagnosis Tool ===
========================================

1. Checking Start and End Points
   Start Location: X=0 Y=0 Z=0
   End Location: X=9 Y=9 Z=0
   Grid Type: 2D
   Grid Size: X=10 Y=10
   [OK] Start cell index: 0
   [OK] End cell index: 99

2. Checking Passable Edge Tags
   Number of passable tags: 1
   - WFC.EdgeType.Door

3. Checking Start Cell
   Passable edges: 1 / 4 (minimum required: 2)
   Traversable: [ERROR] NO
     +X: WFC.EdgeType.Door [PASSABLE]
     -X: WFC.EdgeType.Wall [BLOCKED]
     +Y: WFC.EdgeType.Wall [BLOCKED]
     -Y: WFC.EdgeType.Wall [BLOCKED]

... (更多信息)

7. Diagnosis Summary
   [ISSUE] Start cell tile is not traversable
   [SOLUTION]:
      1. Check start tile edge tags are correct
      2. Ensure tile has at least 2 passable edges
      3. Or lower the MinPassableEdges parameter
```

## 常见问题和解决方案

### 问题 1：起点或终点不可通过

**诊断信息**：
```
[ISSUE] Start cell tile is not traversable
Passable edges: 1 / 4 (minimum required: 2)
```

**原因**：
- 起点/终点的 Tile 的可通过边缘太少（只有 1 个，但需要至少 2 个）

**解决方案**：
1. **方案 A**：修改 Tile 的边缘标签
   - 打开 Tile 资产
   - 找到 Edge Types
   - 将更多边缘改为 `WFC.EdgeType.Door`
   - 例如：将 `+Y` 也改为 Door

2. **方案 B**：降低 MinPassableEdges
   - 在约束配置中
   - 将 `Min Passable Edges` 从 2 改为 1

### 问题 2：起点/终点无法访问邻居

**诊断信息**：
```
[ISSUE] Start cell cannot access any neighbors
Accessible neighbors: 0
```

**原因**：
- 起点虽然有可通过边缘，但相邻的 Tile 没有匹配的可通过边缘

**解决方案**：
1. 检查起点周围的 Tile
2. 确保相邻 Tile 有对应方向的可通过边缘
3. 例如：
   - 起点 +X 方向是 Door
   - 那么右边的 Tile 的 -X 方向也必须是 Door

### 问题 3：标签不匹配

**诊断信息**：
```
Number of passable tags: 1
- WFC.EdgeType.Door

Edge Types:
  +X: WFC.EdgeType.Corridor [BLOCKED]
```

**原因**：
- Tile 的边缘标签是 `Corridor`
- 但你只配置了 `Door` 为可通过标签
- `Corridor` 没有被认为是可通过的

**解决方案**：
在约束配置中添加更多可通过标签：
```
Passable Edge Tags:
  - WFC.EdgeType.Door
  - WFC.EdgeType.Corridor  ← 添加这个
```

## 其他诊断节点

### Print All Tile Edge Tags

打印所有 Tile 的边缘标签信息：

```
Print All Tile Edge Tags
    └─ Generator Component: Self → WFC Generator
```

查看日志，可以看到：
```
=== All Tile Edge Tags ===
Tile Asset: MyWallTile
  TileDef [0]:
    +X: WFC.EdgeType.Wall
    -X: WFC.EdgeType.Wall
    +Y: WFC.EdgeType.Door
    -Y: WFC.EdgeType.Door
```

### Check Cell Traversability

检查特定单元格是否可通过：

```
Check Cell Traversability
    ├─ Generator Component: Self → WFC Generator
    ├─ Cell Index: 0
    ├─ Passable Edge Tags: [你的标签]
    ├─ Min Passable Edges: 2
    ├─ Out Passable Edge Count (输出)
    └─ Out Edge Info (输出)
```

### Get Accessible Neighbors

获取某个单元格可访问的邻居：

```
Get Accessible Neighbors
    ├─ Generator Component: Self → WFC Generator
    ├─ Cell Index: 0
    ├─ Passable Edge Tags: [你的标签]
    └─ Out Accessible Neighbors (输出)
```

## 完整诊断流程示例

```
Event BeginPlay
    ↓
Run New Generation
    └─ New Seed: 12345
    ↓
Wait 1 second (Delay)
    ↓
Diagnose Pathfinding Issue
    ├─ Generator Component: WFCGenerator
    ├─ Passable Edge Tags: [WFC.EdgeType.Door]
    ├─ Start Location: (0, 0, 0)
    ├─ End Location: (9, 9, 0)
    └─ Min Passable Edges: 2
    ↓
Validate Maze Path
    ├─ Generator Component: WFCGenerator
    ├─ Passable Tags: [WFC.EdgeType.Door]
    ├─ Start Location: (0, 0, 0)
    ├─ End Location: (9, 9, 0)
    ├─ Out Path Length: PathLength
    └─ Return Value: HasPath
    ↓
Branch (HasPath?)
    ├─ True → Print String: "Path Found! Length: " + PathLength
    └─ False → Print String: "No Path Found! Check Output Log for diagnosis"
```

## 快速检查清单

在查看诊断日志之前，快速检查：

- [ ] **WFC Asset 中添加了约束**
   - Config → Constraints → + → WFCPathfindingConstraint
   - Enable Pathfinding: ?
   
- [ ] **配置了可通过标签**
   - Passable Edge Tags 不为空
   - 包含了所有应该可通过的标签
   
- [ ] **Tile 的边缘标签正确**
   - 打开几个 Tile 资产
   - 检查 Edge Types 设置
   
- [ ] **起点终点在网格范围内**
   - 10×10 网格：坐标应该是 0-9
   - 不要用 (10, 10, 0)，应该用 (9, 9, 0)

## 总结

1. **添加 Diagnose Pathfinding Issue 节点**到你的 Blueprint
2. **运行游戏**
3. **查看 Output Log**
4. **根据诊断信息修复问题**

诊断工具会告诉你：
- 起点和终点是否有效
- Tile 是否可通过
- 为什么找不到路径
- 如何修复问题

祝你调试顺利！??
