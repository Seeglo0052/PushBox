# ?? WFC 寻路约束 - 快速参考卡

## 3步设置

### 1?? 添加约束
```
WFC Asset → Config → Constraints → + → WFCPathfindingConstraint
```

### 2?? 必填参数
```
? Enable Pathfinding
?? Passable Edge Tags: WFC.EdgeType.Door
?? Start Cell Location: (0, 0, 0)
?? End Cell Location: (9, 9, 0)
```

### 3?? 配置 Tile
```
Tile Asset → Edge Types → 设置为 Door/Wall
```

## 常用配置

### 2D 迷宫（10×10）
```yaml
Enable: ?
Tags: [Door]
Start: (0,0,0)
End: (9,9,0)
MinEdges: 2
Frequency: 10
```

### 3D 地下城（10×10×3）
```yaml
Enable: ?
Tags: [Door, Stairs, Corridor]
Start: (0,0,0)
End: (9,9,2)
MinEdges: 2
Frequency: 15
```

### 大型地图（50×50）
```yaml
Enable: ?
Tags: [Door, Corridor]
Start: (0,0,0)
End: (49,49,0)
MinEdges: 2
Frequency: 30  ← 提高以优化性能
```

## 参数速查

| 参数 | 推荐值 | 说明 |
|------|--------|------|
| **Min Passable Edges** | 2 | 1=允许死胡同, 2=走廊, 3+=房间 |
| **Validation Frequency** | 10-30 | 越小=越准确越慢 |
| **Max Iterations** | 10000 | 大网格可增加到 20000 |

## Tile 边缘标签

### 必须创建的 Tag
```
WFC.EdgeType.Door      ← 可通过
WFC.EdgeType.Corridor  ← 可通过
WFC.EdgeType.Wall      ← 不可通过
```

### Tile 设置示例

**走廊**（可通过）：
```
+X: Door, -X: Door
其他: Wall
→ 2个可通过边缘 ?
```

**房间**（可通过）：
```
+X: Door, -X: Door, +Y: Door, -Y: Door
→ 4个可通过边缘 ?
```

**死胡同**（不可通过）：
```
+X: Door
其他: Wall
→ 1个可通过边缘 ?
```

## 故障排查

### 约束不起作用？
- [ ] 在 WFC Asset 中添加了约束
- [ ] Enable Pathfinding 已勾选
- [ ] Passable Edge Tags 不为空
- [ ] 起点终点在网格范围内

### 找不到路径？
- [ ] Tile 边缘标签设置正确
- [ ] 有足够的可通过 Tile
- [ ] Min Passable Edges 不要太高

### 生成太慢？
- [ ] 增加 Validation Frequency
- [ ] 减少约束检查频率

## 日志检查

正常日志应显示：
```
LogWFC: PathfindingConstraint initialized: Start=(0,0,0), End=(9,9,0)
LogWFC: PathfindingConstraint: Valid path found with 15 cells
```

错误日志：
```
LogWFC: Warning: No valid path found
→ 检查 Tile 配置和可通过标签
```

## Blueprint 函数（可选）

### 快速添加
```
Add Simple Maze Pathfinding
  ├─ Generator Component
  ├─ Start Location: (0,0,0)
  ├─ End Location: (10,10,0)
  └─ Door Tag
```

### 验证路径
```
Validate Maze Path
  ├─ Generator Component
  ├─ Passable Tags
  ├─ Start Location
  ├─ End Location
  └─ Out Path Length
```

## 性能表

| 网格大小 | Frequency | 迭代次数 |
|----------|-----------|----------|
| 10×10 | 5-10 | 5000 |
| 25×25 | 15-20 | 10000 |
| 50×50 | 25-30 | 15000 |
| 100×100 | 50+ | 20000 |
| 10×10×5 (3D) | 15-20 | 15000 |

## 多人游戏

### 自动同步 ?
```cpp
// 服务器
WFCGenerator->SetSeedAndRun(12345);

// 客户端
// 自动使用相同种子，生成相同迷宫
```

### 关键：不影响 RandomStream
- 约束只"禁止" Tile
- 不改变随机选择逻辑
- 确保确定性生成

## 记住

1. **最简单**：在 WFC Asset 中配置
2. **必须**：配置 Tile 边缘标签
3. **调试**：查看日志输出
4. **优化**：调整 Validation Frequency

---

## 详细文档

?? `PathfindingConstraint_EditorGuide_CN.md` - 完整图文教程
?? `FinalSummary_CN.md` - 功能总结
?? `PathfindingImplementationSummary_CN.md` - 技术细节

---

快速参考完成！保存此文件以便随时查看。??
