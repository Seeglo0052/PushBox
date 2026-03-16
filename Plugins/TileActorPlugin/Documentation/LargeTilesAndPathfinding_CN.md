# 大型 Tile 和寻路约束 - 技术说明

## ?? 大型 Tile 的定义

**大型 Tile** 是指占用多个网格单元格的 Tile，例如：
- **1×1×2** (斜坡/楼梯)
- **2×2×1** (大房间)
- **3×3×3** (巨型房间)

## ?? 内部连通性规则

### 核心概念

**大型 Tile 内部的单元格之间应该是连通的。**

这意味着：
- 从大型 Tile 的一个单元格应该能走到另一个单元格
- 不需要通过外部路径

### 实现方式：空边缘标签

**空边缘标签**（未设置标签）表示：
- 这是大型 Tile 的**内部边缘**
- 内部边缘**自动视为可通过**
- 无需在 `Passable Edge Tags` 中配置

### 代码实现

```cpp
bool UWFCPathfindingConstraint::IsEdgePassable(const FGameplayTag& EdgeTag) const
{
    // 空标签 = 大型 Tile 内部 = 可通过
    if (!EdgeTag.IsValid())
    {
        return true;
    }

    // 检查是否在配置的可通过标签列表中
    return PassableEdgeTags.HasTag(EdgeTag);
}
```

## ?? 配置示例

### 示例 1：斜坡 Tile (1×1×2)

#### 用途
从 Z=0 层走到 Z=1 层

#### 边缘标签配置

**底层单元格 (Z=0)**：
```
+X: WFC.EdgeType.Wall      ← 外部墙
-X: WFC.EdgeType.Wall      ← 外部墙
+Y: WFC.EdgeType.Road      ← 入口（可通过）
-Y: WFC.EdgeType.Wall      ← 外部墙
+Z: <Empty>                ← 指向顶层（内部，自动可通过）
-Z: WFC.EdgeType.Pillar    ← 地基（不可通过）
```

**顶层单元格 (Z=1)**：
```
+X: WFC.EdgeType.Wall      ← 外部墙
-X: WFC.EdgeType.Wall      ← 外部墙
+Y: WFC.EdgeType.Wall      ← 外部墙
-Y: WFC.EdgeType.Road      ← 出口（可通过）
+Z: WFC.EdgeType.Open      ← 顶部开放（可能可通过）
-Z: <Empty>                ← 指向底层（内部，自动可通过）
```

#### 行为
1. 从外部通过 +Y 进入底层
2. 通过内部边缘 (+Z) 从底层走到顶层
3. 从顶层通过 -Y 离开

### 示例 2：大房间 Tile (3×3×1)

#### 用途
创建大型开放空间

#### 边缘标签配置

**外围单元格**：
- 外部边缘：`WFC.EdgeType.Wall` 或 `WFC.EdgeType.Door`
- 内部边缘：`<Empty>` (自动可通过)

**中心单元格**：
- 所有边缘：`<Empty>` (全部内部，全部可通过)

#### 行为
- 可以在大房间内部自由移动
- 通过门进出房间

## ? 验证大型 Tile 配置

### 使用诊断工具

```
Diagnose Pathfinding Issue
    ├─ Generator Component: WFCGenerator
    ├─ Passable Edge Tags: [WFC.EdgeType.Road]
    ├─ Start Location: (0, 0, 0)  ← 底层
    ├─ End Location: (0, 0, 1)    ← 顶层
    └─ Min Passable Edges: 2
```

### 查看日志

**正确的日志输出**：
```
LogWFC: 3. Checking Start Cell
LogWFC:    Passable edges: 2 / 6
LogWFC:      +Y: WFC.EdgeType.Road [OK]       ← 外部入口
LogWFC:      +Z: <Empty/Interior> [OK]        ← 内部连接
LogWFC:      -Z: WFC.EdgeType.Pillar [X]
LogWFC:      ...

LogWFC: 4. Checking End Cell
LogWFC:    Passable edges: 2 / 6
LogWFC:      -Y: WFC.EdgeType.Road [OK]       ← 外部出口
LogWFC:      -Z: <Empty/Interior> [OK]        ← 内部连接
LogWFC:      ...
```

### 错误的配置

**症状**：
```
LogWFC: +Z: WFC.EdgeType.Interior [X]  ← 错误：设置了标签但不在可通过列表中
```

**解决方案**：
- **方案 A**：将边缘标签改为空（推荐）
- **方案 B**：添加 `WFC.EdgeType.Interior` 到 `Passable Edge Tags`

## ?? 最佳实践

### DO ?

1. **大型 Tile 内部边缘留空**
   ```
   内部边缘 → 不设置标签 → 自动可通过
   ```

2. **外部边缘设置明确标签**
   ```
   外部边缘 → WFC.EdgeType.Door / Wall / Road
   ```

3. **确保入口和出口正确**
   ```
   斜坡底层 → 至少一个可通过的外部边缘（入口）
   斜坡顶层 → 至少一个可通过的外部边缘（出口）
   ```

### DON'T ?

1. **不要给内部边缘设置标签**
   ```
   ? 内部边缘 → WFC.EdgeType.Interior
   ? 内部边缘 → <Empty>
   ```

2. **不要忘记配置外部边缘**
   ```
   ? 所有边缘都是空的
   ? 外部边缘有明确标签，内部边缘为空
   ```

3. **不要将内部标签添加到 Passable Edge Tags**
   ```
   ? Passable Edge Tags: [Door, Interior]
   ? Passable Edge Tags: [Door, Road]
      (内部边缘自动可通过，无需配置)
   ```

## ?? 调试技巧

### 1. 检查大型 Tile 是否被正确识别

运行诊断工具，查看是否有 `<Empty/Interior>` 标记：
```
LogWFC: +Z: <Empty/Interior> [OK]
```

### 2. 验证内部连通性

对于 1×1×2 斜坡：
- 检查底层单元格的 +Z 邻居
- 应该包含顶层单元格

### 3. 测试跨层路径

```
Start: (5, 5, 0)  ← 底层
End:   (5, 5, 1)  ← 顶层（斜坡位置）
```

应该能找到路径（如果斜坡配置正确）。

## ?? 性能考虑

### 空标签检查的性能影响

**几乎没有影响**：
```cpp
if (!EdgeTag.IsValid())  // 非常快的检查
{
    return true;
}
```

### 建议

- 空标签检查比标签查找**更快**
- 尽量使用空标签表示内部边缘
- 避免为内部边缘创建特殊标签

## ?? 总结

### 核心规则

1. **大型 Tile 内部边缘 = 空标签 = 自动可通过**
2. **外部边缘 = 明确标签 = 需要配置**
3. **无需为空标签配置 Passable Edge Tags**

### 为什么这样设计？

**简化配置**：
- 用户不需要为每个大型 Tile 创建特殊的"内部"标签
- 不需要记得将"内部"标签添加到可通过列表
- 空标签自然表示"这是内部，当然可以通过"

**性能优化**：
- 空标签检查比标签匹配更快
- 减少标签系统的复杂度

**直观理解**：
- 没有标签 = 没有阻碍 = 可以通过
- 符合直觉的设计

---

**如有疑问，请参考**：
- `PathfindingConstraint_EditorGuide_CN.md` - 完整配置指南
- `PathfindingDiagnosisGuide_CN.md` - 调试工具使用
