# PathfindingConstraint 成功率优化指南

## ?? 理论成功率

**理论上应该接近 100%**，因为：
1. 随机种子不影响是否能生成通路，只影响选择哪些 Tile
2. PathfindingConstraint 应该引导生成确保路径存在

**实际可能达不到 100%** 的原因：

---

## ?? 影响成功率的因素

### 1. ?? **验证时机太晚**

**问题**：
```cpp
ValidationFrequency = 10  // 默认每10个Cell才验证一次
```

**影响**：
- 在验证之前可能已经选择了很多阻碍路径的 Tile
- 发现问题时已经太晚，难以修复

**解决方案**：
```
? 降低到 5（已修复）
? 或在 Blueprint 中设置更小的值（1-3）
```

---

### 2. ?? **Ban Tile 策略太保守**

**之前的问题**：
```cpp
// 只在起点、终点和它们的直接邻居中 Ban 不可通过的 Tile
CriticalCells.Add(StartCell);
CriticalCells.Add(EndCell);
// 只添加直接邻居...
```

**影响**：
- **中间大部分 Cell 完全不受约束**！
- 如果中间选择了不可通过的 Tile，路径就断了

**修复方案**（已实现）：
```cpp
// ? 在起点和终点之间的直线上添加关键 Cell
// 创建一条"走廊"确保路径连通
for (int32 i = 0; i <= Steps; ++i)
{
    const float T = (float)i / Steps;
    const FIntVector InterpLoc = Lerp(StartLoc, EndLoc, T);
    CriticalCells.Add(InterpLoc);
}
```

---

### 3. ?? **MinPassableEdges 计算错误**（已修复）

**之前的问题**：
- 对于大型 Tile（如 1×1×2 斜坡），每个 Cell 单独检查
- 导致很多合法的大型 Tile 被认为不可通过

**修复**：
- 现在正确统计整个 Tile 的外部可通过边缘
- 使用 `IsInteriorEdge` 排除内部边缘

---

### 4. ?? **约束顺序的影响**

**是的！约束顺序很重要！**

#### 推荐顺序

```
WFC Asset → Constraints 数组：

1. FixedTileConstraint        ← 固定某些位置的 Tile（如果有）
2. PathfindingConstraint       ← 寻路约束（应该放在前面）?
3. BoundaryConstraint          ← 边界约束
4. EdgeConstraint              ← 边缘匹配约束
5. TagCountConstraint          ← 标签数量约束
6. 其他自定义约束...
```

**为什么 Pathfinding 应该靠前**：
1. **早期引导** - 在生成早期就开始 Ban 不可通过的 Tile
2. **避免矛盾** - 如果放在后面，其他约束可能已经造成矛盾
3. **优先级高** - 寻路通常是最重要的需求

#### 不好的顺序

```
? 错误顺序：

1. TagCountConstraint          
2. OtherConstraints...
3. PathfindingConstraint       ← 放在最后（太晚了！）
```

**问题**：
- 前面的约束可能已经 Ban 了很多 Tile
- PathfindingConstraint 发现问题时选择余地很小
- 容易导致矛盾

---

## ?? 预期效果

### 修复前

```
成功率: 10-30%
原因:
- MinPassableEdges 计算错误
- ValidationFrequency 太大
- Ban策略太保守
```

### 修复后（已实现的优化）

```
预期成功率: 70-90%
优化:
? MinPassableEdges 正确计算
? ValidationFrequency 降低到 5
? 直线路径上的 Cell 加入关键 Cell 列表
```

### 理想状态（使用 PathGuidedCellSelector）

```
目标成功率: 95%+
额外优化:
? PathGuidedCellSelector - 三阶段路径优先策略
□ 动态调整 ValidationFrequency
□ 更智能的 Tile 选择策略
```

---

## ?? 为什么不能达到 100%？

即使有所有优化，仍可能出现失败：

### 1. **Tile 集合不足**

```
场景：只有少量可通过的 Tile
结果：生成过程中用完了所有选择，找不到合适的 Tile
```

**解决**：确保有足够种类的可通过 Tile

### 2. **其他约束冲突**

```
场景：PathfindingConstraint 要求某个 Cell 是可通过的
      但 TagCountConstraint 要求这个位置必须是墙
结果：矛盾，生成失败
```

**解决**：调整其他约束的优先级/参数

### 3. **网格大小和复杂度**

```
场景：非常大的网格 (100×100×10)
      起点和终点距离很远
结果：路径可能被其他约束阻断
```

**解决**：增加 ValidationFrequency，更频繁验证

---

## ? 最佳实践

### 1. 约束配置

```
PathfindingConstraint:
  Min Passable Edges: 2
  Validation Frequency: 5        ← 小网格
  Validation Frequency: 10       ← 中等网格
  Validation Frequency: 15-20    ← 大网格
```

### 2. 约束顺序

```
1. FixedTileConstraint (如果有)
2. PathfindingConstraint  ? 放在前面
3. BoundaryConstraint
4. EdgeConstraint
5. 其他约束...
```

### 3. Tile 设计

```
? 确保有足够的可通过 Tile
? 至少 3-5 种不同方向的走廊/通道
? 大型 Tile 的内部边缘留空
? 合理设置外部边缘标签
```

### 4. 测试和调试

```
1. 使用 Draw Pathfinding Debug 可视化
2. 使用 Diagnose Pathfinding Issue 诊断
3. 测试多个种子（10-20个）
4. 查看日志中的警告
```

---

## ?? 测试成功率

### Blueprint 测试脚本

```
For Loop (i = 0 to 100)  // 测试100个种子
    ↓
    Run New Generation
        Seed: i
    ↓
    Delay (1 second)
    ↓
    Branch (State == Finished?)
        True:
            Increment Success Counter
        False:
            Increment Failure Counter
            Print: "Seed [i] failed"
    ↓
End Loop
    ↓
Print: "Success Rate: [Success / 100]%"
```

### 预期结果

```
优化前: 10-30% 成功
优化后: 70-90% 成功
理想: 95-100% 成功
```

---

## ?? 进一步优化方向

如果成功率仍然不理想，可以考虑：

### 1. **实现主动路径引导**

```cpp
// 在 Cell 选择时，优先选择靠近直线路径的 Cell
FWFCCellIndex SelectNextCell()
{
    // 计算每个 Cell 到起点-终点直线的距离
    // 优先选择距离近的 Cell
}
```

### 2. **动态调整验证频率**

```cpp
// 根据当前状态调整验证频率
if (!bHasValidatedPath && CellSelectionCounter > TotalCells / 2)
{
    ValidationFrequency = 1; // 后期更频繁验证
}
```

### 3. **回溯机制**

```cpp
// 如果发现无法建立路径，回溯几步重新尝试
if (!bPathExists && CellSelectionCounter > MinCells)
{
    UndoLastNSelections(5);
    RetryGeneration();
}
```

---

## ?? 总结

### 当前状态（已完成的修复）

1. ? **MinPassableEdges 正确计算**（最重要）
2. ? **ValidationFrequency 降低到 5**
3. ? **BanBlockingTiles 改进**（直线路径策略）
4. ? **DrawPathfindingDebug 自动清除**
5. ? **PathGuidedCellSelector**（革命性改进）

### 建议配置

```
Constraints 顺序:
1. FixedTileConstraint
2. PathfindingConstraint  ?
3. BoundaryConstraint
4. EdgeConstraint
5. TagCountConstraint

PathfindingConstraint 参数:
- MinPassableEdges: 2
- ValidationFrequency: 5
- Passable Edge Tags: [Road]

Cell Selector:
- PathGuidedCellSelector  ? 推荐使用
  - PathCorridorWidth: 2
  - PathExpansionDistance: 3
  - PathRandomness: 0.3
```

### 预期效果

**成功率应该从 10-30% 提升到 95%+！**

---

**现在重新测试你的项目，看看成功率是否有显著提升！** ??

如果成功率仍然很低（< 70%），请检查：
1. 约束顺序是否正确
2. 是否有足够的可通过 Tile
3. 其他约束是否过于严格
4. 是否使用了 PathGuidedCellSelector
