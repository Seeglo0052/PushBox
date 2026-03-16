# CellSelector 对寻路成功率的影响分析

## ?? CellSelector 的作用

CellSelector 决定 WFC 算法**下一个选择哪个 Cell 来坍缩**。

### 当前实现的 Selector

#### 1. **EntropyCellSelector**（熵选择器）

```cpp
FWFCCellIndex SelectNextCell()
{
    float MinEntropy = MAX_FLT;
    FWFCCellIndex BestCellIndex = INDEX_NONE;

    for (FWFCCellIndex CellIndex = 0; CellIndex < Generator->GetNumCells(); ++CellIndex)
    {
        const FWFCCell& Cell = Generator->GetCell(CellIndex);
        
        if (Cell.HasSelectionOrNoCandidates())
        {
            continue; // 跳过已选择的 Cell
        }

        // 计算熵 + 少量随机偏差
        const float Entropy = CalculateShannonEntropy(Cell) + Generator->GetRandomFloat() * RandomDeviation;
        
        if (Entropy < MinEntropy)
        {
            MinEntropy = Entropy;
            BestCellIndex = CellIndex;
        }
    }

    return BestCellIndex;
}
```

**特点**：
- **优先选择熵最低的 Cell**（候选 Tile 最少的）
- **添加少量随机偏差**（RandomDeviation = 0.001）避免完全确定性
- **倾向于**先解决"最困难"的位置

---

## ?? 对寻路的影响

### ? 好消息

**EntropyCellSelector 本身不会导致寻路失败**，因为：

1. **使用确定性随机数**
   ```cpp
   const float Entropy = CalculateShannonEntropy(Cell) + Generator->GetRandomFloat() * RandomDeviation;
   ```
   - `Generator->GetRandomFloat()` 使用你设置的 RandomStream
   - **同一个种子 = 同样的 Cell 选择顺序**

2. **不影响 Tile 选择**
   - CellSelector 只决定"选择哪个 Cell"
   - 具体选择哪个 Tile 由权重和随机数决定
   - PathfindingConstraint 可以 Ban 不合适的 Tile

### ?? 但是...

**Cell 选择顺序确实会间接影响成功率**！

---

## ?? 为什么 Cell 选择顺序很重要？

### 场景分析

假设有一个 10×10 网格，起点 (0,0)，终点 (9,9)：

#### 情况 A：Entropy 先选择角落

```
步骤 1: 选择 (0,0) - 熵低（边缘位置，候选少）
步骤 2: 选择 (9,9) - 熵低（另一个角落）
步骤 3: 选择 (0,9) - 熵低
步骤 4: 选择 (9,0) - 熵低
...
步骤 50: 才选择中间的 (5,5)
```

**问题**：
- **起点和终点很早就被选择了**
- 但**中间的路径很晚才开始形成**
- 如果中间选择了阻碍 Tile，**很难修正**

#### 情况 B：如果有"路径优先"选择器

```
步骤 1: 选择 (0,0) - 起点
步骤 2: 选择 (1,0) - 靠近起点
步骤 3: 选择 (2,0) - 沿路径
...
步骤 10: 选择 (9,9) - 终点
```

**优势**：
- **逐步建立路径**
- 更容易**及时发现并修正**阻碍
- PathfindingConstraint 更容易引导

---

## ?? 实验对比

### 测试场景

- 网格：10×10×2
- 起点：(0,0,0)
- 终点：(9,9,1)
- Tile 类型：30% 可通过，70% 阻碍

### 预测结果

| Selector 类型 | 预期成功率 | 原因 |
|--------------|----------|------|
| **Entropy**（当前） | 60-80% | 可能先选择边缘，路径形成晚 |
| **路径优先** | 85-95% | 逐步建立路径，容易引导 |
| **Random** | 40-60% | 完全随机，很难引导 |

---

## ?? 改进建议

### 方案 A：实现路径优先的 CellSelector

创建一个新的 CellSelector，优先选择靠近直线路径的 Cell：

```cpp
class UWFCPathAwareCellSelector : public UWFCCellSelector
{
public:
    FIntVector StartLocation;
    FIntVector EndLocation;
    
    FWFCCellIndex SelectNextCell() override
    {
        float MinScore = MAX_FLT;
        FWFCCellIndex BestCell = INDEX_NONE;
        
        for (FWFCCellIndex CellIndex = 0; CellIndex < Generator->GetNumCells(); ++CellIndex)
        {
            const FWFCCell& Cell = Generator->GetCell(CellIndex);
            
            if (Cell.HasSelectionOrNoCandidates())
            {
                continue;
            }
            
            // 计算到直线路径的距离
            FIntVector CellLoc = Grid->GetLocationForCellIndex(CellIndex);
            float DistanceToPath = CalculateDistanceToLine(CellLoc, StartLocation, EndLocation);
            
            // 熵（候选数）
            float Entropy = CalculateShannonEntropy(Cell);
            
            // 综合评分：距离 + 熵
            float Score = DistanceToPath * 0.7f + Entropy * 0.3f;
            
            if (Score < MinScore)
            {
                MinScore = Score;
                BestCell = CellIndex;
            }
        }
        
        return BestCell;
    }
};
```

**优势**：
- 优先选择路径附近的 Cell
- 逐步建立连接
- 更容易被 PathfindingConstraint 引导

### 方案 B：混合策略

前期使用路径优先，后期使用熵优先：

```cpp
FWFCCellIndex SelectNextCell() override
{
    int32 NumSelected = Generator->GetNumSelectedCells();
    int32 TotalCells = Generator->GetNumCells();
    float Progress = (float)NumSelected / TotalCells;
    
    if (Progress < 0.3f)
    {
        // 前30%：路径优先
        return SelectPathPriorityCell();
    }
    else
    {
        // 后70%：熵优先
        return SelectEntropyCell();
    }
}
```

### 方案 C：增加 EntropyCellSelector 的随机偏差

当前 `RandomDeviation = 0.001` 很小，可以增加：

```cpp
UWFCEntropyCellSelector::UWFCEntropyCellSelector()
    : RandomDeviation(0.05f)  // 从 0.001 增加到 0.05
{
}
```

**效果**：
- 更大的随机性
- 不同种子下 Cell 选择顺序变化更大
- 可能提高某些种子的成功率

---

## ?? 建议测试

### 测试 1：增加 RandomDeviation

**步骤**：
1. 修改 `EntropyCellSelector` 的 `RandomDeviation`
2. 从 0.001 增加到 0.01, 0.05, 0.1
3. 测试 100 个种子的成功率

**预期**：
- 0.001：成功率 X%
- 0.01：成功率 X+5%
- 0.05：成功率 X+10%
- 0.1：成功率可能下降（太随机）

### 测试 2：对比不同 Selector（如果有）

**步骤**：
1. 在 WFC Asset 中切换 CellSelector
2. 测试相同的种子
3. 对比成功率

---

## ?? 结论

### 当前状态

1. ? **EntropyCellSelector 不是主要问题**
   - 它使用确定性随机数
   - 同一种子 = 相同的选择顺序

2. ?? **但 Cell 选择顺序确实有影响**
   - 熵优先可能导致路径形成较晚
   - 如果中间出现阻碍，较难修正

3. ? **PathfindingConstraint 的优化更重要**
   - 增加验证频率（已完成）
   - 改进 Ban 策略（已完成）
   - 直线路径上的关键 Cell（已完成）

### 优先级建议

**优先级 1**（已完成）：
- ? 修复 MinPassableEdges 计算
- ? 降低 ValidationFrequency
- ? 改进 BanBlockingTiles

**优先级 2**（可选优化）：
- ?? 增加 EntropyCellSelector 的 RandomDeviation (0.05)
- ?? 实现路径优先的 CellSelector
- ?? 测试不同策略的效果

**优先级 3**（高级优化）：
- ?? 实现混合选择策略
- ?? 动态调整选择策略
- ?? 机器学习优化 Cell 选择

---

## ?? 快速实验

如果你想立即测试 RandomDeviation 的影响：

### 修改 EntropyCellSelector

找到文件：
```
Plugins/WFC/Source/WFC/Private/Core/CellSelectors/WFCEntropyCellSelector.cpp
```

修改构造函数：
```cpp
UWFCEntropyCellSelector::UWFCEntropyCellSelector()
    : RandomDeviation(0.05f)  // 从 0.001 改为 0.05
{
}
```

**测试**：
- 运行 50-100 个不同种子
- 统计成功率
- 对比修改前后的差异

---

**总结**：CellSelector 有影响，但不是主要问题。当前的优化（MinPassableEdges、ValidationFrequency、BanBlockingTiles）更重要！
