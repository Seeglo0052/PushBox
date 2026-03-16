# MinPassableEdges 修复说明

## ?? 问题描述

**之前的错误实现**：
```cpp
// 错误：只检查当前 Cell 的边缘
for (FWFCGridDirection Direction = 0; Direction < Grid->GetNumDirections(); ++Direction)
{
    const FGameplayTag EdgeType = TileAsset->GetTileDefEdgeType(AssetTile->TileDefIndex, Direction);
    if (IsEdgePassable(EdgeType))
    {
        PassableEdgeCount++;
    }
}
return PassableEdgeCount >= MinPassableEdges;  // 错误！
```

**问题**：
- 对于 **1×1×2 斜坡**：
  - 底层 Cell：只有 1 个可通过边缘（入口）→ 不满足 MinPassableEdges=2
  - 顶层 Cell：只有 1 个可通过边缘（出口）→ 不满足 MinPassableEdges=2
  - **结果**：整个斜坡 Tile 被认为不可通过！?

## ? 正确实现

```cpp
// 正确：统计整个 Tile 的所有外部可通过边缘
int32 TotalPassableExternalEdges = 0;
const int32 NumTileDefs = TileAsset->GetNumTileDefs();

for (int32 TileDefIdx = 0; TileDefIdx < NumTileDefs; ++TileDefIdx)
{
    for (FWFCGridDirection Direction = 0; Direction < Grid->GetNumDirections(); ++Direction)
    {
        // 关键：跳过内部边缘！
        if (TileAsset->IsInteriorEdge(TileDefIdx, Direction))
        {
            continue; // 不计算内部边缘
        }
        
        const FGameplayTag EdgeType = TileAsset->GetTileDefEdgeType(TileDefIdx, LocalDirection);
        if (IsEdgePassable(EdgeType))
        {
            TotalPassableExternalEdges++;
        }
    }
}

return TotalPassableExternalEdges >= MinPassableEdges;  // 正确！
```

**现在对于 1×1×2 斜坡**：
- 底层外部边缘：+Y (Road) = 1 个可通过
- 顶层外部边缘：-Y (Road) = 1 个可通过
- **总计外部可通过边缘 = 2** ?
- 满足 MinPassableEdges=2 → **Tile 可通过！** ?

---

## ?? 对比示例

### 示例 1：1×1×2 斜坡 Tile

```
底层 (Cell 0):
  +X: Wall        ← 外部，不可通过
  -X: Wall        ← 外部，不可通过
  +Y: Road        ← 外部，可通过 ?
  -Y: Wall        ← 外部，不可通过
  +Z: <Empty>     ← 内部（指向顶层），跳过
  -Z: Pillar      ← 外部，不可通过

顶层 (Cell 1):
  +X: Wall        ← 外部，不可通过
  -X: Wall        ← 外部，不可通过
  +Y: Wall        ← 外部，不可通过
  -Y: Road        ← 外部，可通过 ?
  +Z: Open        ← 外部，不可通过（假设）
  -Z: <Empty>     ← 内部（指向底层），跳过
```

**错误实现**：
- 底层：1 个可通过边缘 < 2 → ?
- 顶层：1 个可通过边缘 < 2 → ?
- **结果**：Tile 不可通过 ?

**正确实现**：
- 外部可通过边缘：+Y (底层) + -Y (顶层) = 2 ?
- **结果**：Tile 可通过 ?

---

### 示例 2：普通走廊 Tile (1×1×1)

```
+X: Road        ← 外部，可通过 ?
-X: Road        ← 外部，可通过 ?
+Y: Wall        ← 外部，不可通过
-Y: Wall        ← 外部，不可通过
+Z: Wall        ← 外部，不可通过
-Z: Wall        ← 外部，不可通过
```

**错误实现**：
- 2 个可通过边缘 >= 2 → ?

**正确实现**：
- 外部可通过边缘：2 ?

**结果**：两种实现都正确（因为只有 1 个 TileDef）

---

### 示例 3：大型房间 Tile (3×3×1)

```
边缘 Cell（例如 (0,0,0)）:
  +X: <Empty>     ← 内部（指向 (1,0,0)），跳过
  -X: Wall        ← 外部，不可通过
  +Y: <Empty>     ← 内部（指向 (0,1,0)），跳过
  -Y: Wall        ← 外部，不可通过
  ...

角落 Door Cell（例如 (0,1,0)）:
  +X: <Empty>     ← 内部，跳过
  -X: Door        ← 外部，可通过 ?
  +Y: <Empty>     ← 内部，跳过
  -Y: <Empty>     ← 内部，跳过
  ...

中心 Cell（例如 (1,1,0)）:
  +X: <Empty>     ← 内部，跳过
  -X: <Empty>     ← 内部，跳过
  +Y: <Empty>     ← 内部，跳过
  -Y: <Empty>     ← 内部，跳过
  +Z: <Empty>     ← 内部，跳过
  -Z: <Empty>     ← 内部，跳过
```

**错误实现**：
- 每个 Cell 可能只有 0-1 个外部可通过边缘
- 大部分 Cell 不满足 MinPassableEdges=2 → ?

**正确实现**：
- 统计所有外部边缘，假设有 4 个 Door
- 外部可通过边缘：4 ?
- **结果**：Tile 可通过 ?

---

## ?? 关键点

### 1. IsInteriorEdge 的作用

```cpp
bool IsInteriorEdge(int32 TileDefIndex, FWFCGridDirection Direction) const
```

- **返回 true**：这条边缘指向同一个 Tile 内的另一个 Cell
- **返回 false**：这条边缘是外部边缘，指向其他 Tile

**示例**：
```
1×1×2 斜坡:
  底层 +Z → 指向顶层 → IsInteriorEdge = true
  顶层 -Z → 指向底层 → IsInteriorEdge = true
  底层 +Y → 指向外部 → IsInteriorEdge = false
  顶层 -Y → 指向外部 → IsInteriorEdge = false
```

### 2. MinPassableEdges 的正确含义

**定义**：一个 **Tile**（无论多大）需要有多少个**外部**可通过边缘才被认为是可通过的。

- **不是**：每个 Cell 需要有多少个可通过边缘
- **而是**：整个 Tile 需要有多少个外部可通过边缘

**推荐值**：
- `MinPassableEdges = 1`：允许死胡同（只有一个入口/出口）
- `MinPassableEdges = 2`：普通走廊/斜坡（有入口和出口）
- `MinPassableEdges = 3+`：房间/交叉口（多个出口）

---

## ?? 性能影响

### 之前（错误）
```cpp
for (Direction in 6 directions)  // O(6)
{
    check edge
}
```
**复杂度**：O(6) per Tile

### 现在（正确）
```cpp
for (TileDefIdx in NumTileDefs)          // 对于 1×1×2: O(2)
{
    for (Direction in 6 directions)      // O(6)
    {
        if (!IsInteriorEdge(...))        // O(1)
        {
            check edge
        }
    }
}
```
**复杂度**：O(NumTileDefs × 6)

**实际影响**：
- 1×1×1 Tile：6 次检查（和之前一样）
- 1×1×2 Tile：12 次检查（但只在约束初始化时）
- 3×3×3 Tile：162 次检查（罕见，且只在初始化时）

**结论**：性能影响很小，因为这个检查只在**约束初始化/Ban Tiles**时执行，不在寻路循环中。

---

## ?? 测试建议

### 测试 1：简单 2D 走廊

```
MinPassableEdges = 2
Tile: 1×1×1 走廊
  +X: Road  ?
  -X: Road  ?
  其他: Wall
```

**预期**：2 个外部可通过边缘 → 可通过 ?

---

### 测试 2：1×1×2 斜坡

```
MinPassableEdges = 2
底层:
  +Y: Road  ?
  +Z: Empty (内部)
  其他: Wall
顶层:
  -Y: Road  ?
  -Z: Empty (内部)
  其他: Wall
```

**预期**：2 个外部可通过边缘 → 可通过 ?

---

### 测试 3：死胡同

```
MinPassableEdges = 2
Tile: 1×1×1
  +X: Road  ?
  其他: Wall
```

**预期**：1 个外部可通过边缘 < 2 → 不可通过 ?

**如何允许**：设置 `MinPassableEdges = 1`

---

### 测试 4：3×3×3 大房间

```
MinPassableEdges = 2
假设有 4 个 Door 在外围
```

**预期**：4 个外部可通过边缘 >= 2 → 可通过 ?

---

## ?? 用户指南更新

### 之前的错误说明

~~"MinPassableEdges: 一个 **单元格** 需要有多少个可通过边缘"~~

### 正确的说明

**MinPassableEdges**: 一个 **Tile**（无论大小）需要有多少个**外部**可通过边缘才被认为是可通过的。

**注意**：
- 对于大型 Tile（如 1×1×2 斜坡），会统计**所有外部边缘**
- **内部边缘**（Tile 内部的连接）**不计算在内**
- 空标签仍然视为可通过（如果它们是外部边缘的话）

---

## ?? 总结

这个修复**彻底解决**了大型 Tile 无法通过寻路验证的问题！

**修复前**：
- 1×1×2 斜坡几乎总是被认为不可通过
- 成功率极低
- 即使成功，也是因为运气好选择了单元格 Tile

**修复后**：
- 正确识别大型 Tile 的可通行性
- 1×1×2 斜坡被正确识别为有 2 个外部可通过边缘
- **成功率应该大幅提升！** ??

---

**现在请重新测试你的项目！** 应该能看到显著的成功率提升。
