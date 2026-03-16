# 寻路可视化调试工具使用指南

## ?? 功能介绍

**Draw Pathfinding Debug** 节点可以在3D世界中**可视化显示寻路结果**，帮助你快速诊断为什么找不到路径。

### 显示内容

- **绿色大球**：起点位置
- **红色大球**：终点位置
- **蓝色线和青色小球**：找到的路径
- **黄色小球**：从起点可以到达的所有单元格
- **红色 X 标记**：找不到路径时显示

---

## ?? 使用方法

### 在 Blueprint 中使用

在你的 WFCTestingActor Blueprint 中添加：

```
Run New Generation
    ↓
Delay (1 second)  ← 等待生成完成
    ↓
Draw Pathfinding Debug  ← 新增的可视化节点
    ├─ Generator Component: Self → WFC Generator
    ├─ Passable Edge Tags: [WFC.EdgeType.Road]
    ├─ Start Location: (0, 0, 0)
    ├─ End Location: (9, 9, 1)
    ├─ Duration: 10.0  ← 显示10秒
    ├─ Draw Path: ?  ← 绘制路径
    └─ Draw Grid: ?  ← 不绘制网格（可选）
    ↓
Print String: "Check the 3D viewport!"
```

---

## ?? 颜色说明

### 找到路径时

```
?? 绿色大球 (50单位半径)
   ↓
?? 蓝色线 + ?? 青色小球 (30单位半径)
   ↓
   ?? 青色小球
   ↓
   ?? 青色小球
   ↓
?? 红色大球 (50单位半径)

显示文字: "PATH FOUND! Length: XX cells"
```

### 找不到路径时

```
?? 绿色大球 (起点)

?? 黄色小球 (从起点可到达的区域)
?? 黄色小球
?? 黄色小球
...

? 红色 X 标记 (在起点和终点中间)

?? 红色大球 (终点)

显示文字: "NO PATH FOUND!"
```

---

## ?? 参数说明

### Generator Component
- **类型**：UWFCGeneratorComponent
- **说明**：你的 WFC 生成器组件
- **获取**：Self → WFC Generator

### Passable Edge Tags
- **类型**：FGameplayTagContainer
- **说明**：可通过的边缘标签
- **重要**：必须和你的约束配置一致
- **示例**：`[WFC.EdgeType.Road]`

### Start Location
- **类型**：FIntVector
- **说明**：起点网格坐标
- **示例**：`(0, 0, 0)`

### End Location
- **类型**：FIntVector
- **说明**：终点网格坐标
- **示例**：`(9, 9, 1)`

### Duration
- **类型**：float
- **说明**：调试图形显示时间（秒）
- **默认值**：5.0
- **推荐**：
  - 快速查看：3-5秒
  - 详细分析：10-30秒
  - `-1`：只显示一帧

### Draw Path
- **类型**：bool
- **说明**：是否绘制找到的路径
- **默认值**：true
- **建议**：? 勾选

### Draw Grid
- **类型**：bool
- **说明**：是否绘制整个网格
- **默认值**：false
- **用途**：
  - 用于查看整体布局
  - 大网格时会很密集，不建议开启
  - 小网格（10×10）可以尝试

---

## ?? 使用场景

### 场景 1：快速验证路径

```blueprint
Run New Generation
    ↓
Draw Pathfinding Debug
    Duration: 5.0
```

**用途**：生成后立即查看是否有路径。

---

### 场景 2：对比不同种子

```blueprint
For Loop (0 到 5)
    ↓
Run New Generation
    New Seed: Loop Index
    ↓
Delay (2 seconds)
    ↓
Draw Pathfinding Debug
    Duration: 3.0
    ↓
Delay (3 seconds)
```

**用途**：快速测试多个种子的生成结果。

---

### 场景 3：详细分析

```blueprint
Run New Generation
    ↓
Delay (1 second)
    ↓
Draw Pathfinding Debug
    Duration: 30.0
    Draw Path: ?
    Draw Grid: ?
    ↓
Diagnose Pathfinding Issue  ← 同时使用诊断工具
```

**用途**：同时使用可视化和诊断日志进行深入分析。

---

## ?? 调试技巧

### 技巧 1：查看可到达区域

当显示 "NO PATH FOUND!" 时，观察**黄色小球**：

```
?? 起点
?????? ← 这些黄色球显示从起点能到达的所有位置
??????
??????

如果黄色区域很小 → 起点被困住了
如果黄色区域很大但不包含终点 → 中间有阻碍
```

### 技巧 2：分段测试

如果长距离找不到路径，尝试分段测试：

```blueprint
// 测试 1：起点到中点
Draw Pathfinding Debug
    Start: (0, 0, 0)
    End: (5, 5, 0)
    Duration: 5.0

Delay (5 seconds)

// 测试 2：中点到终点
Draw Pathfinding Debug
    Start: (5, 5, 0)
    End: (9, 9, 1)
    Duration: 5.0
```

### 技巧 3：查看路径长度

如果找到路径，观察显示的文字：

```
"PATH FOUND! Length: 50 cells"
```

- **长度合理**（< 网格大小 × 2）→ 正常
- **长度太长**（>> 网格大小）→ 路径可能很曲折，考虑优化

### 技巧 4：结合日志使用

同时查看 Output Log 和 3D 可视化：

```
LogWFC: DrawPathfindingDebug: Path found with 25 cells
LogWFC: Drew 15 reachable cells from start (yellow)
```

---

## ?? 常见问题

### 问题 1：看不到任何绘制

**检查清单**：
- ? Duration > 0（不要用 -1）
- ? 相机能看到生成的区域
- ? 生成器已经完成生成（使用 Delay 等待）
- ? WFC Asset 已正确配置

### 问题 2：只看到绿色和红色球，没有路径

**原因**：找不到路径

**解决**：
1. 查看是否有黄色小球
2. 如果没有黄色球 → 起点可能没有 Tile 或 Tile 不可通过
3. 使用 `Diagnose Pathfinding Issue` 节点查看详细信息

### 问题 3：路径很奇怪/绕远

**可能原因**：
1. 某些 Tile 的边缘标签配置不正确
2. MinPassableEdges 设置太高
3. 缺少某些类型的 Tile

**解决**：
1. 使用 `Print All Tile Edge Tags` 检查配置
2. 降低 MinPassableEdges
3. 增加 Tile 种类

### 问题 4：绘制太密集看不清

**解决方案**：

```blueprint
Draw Pathfinding Debug
    Duration: 10.0
    Draw Path: ?
    Draw Grid: ?  ← 关闭网格绘制
```

或者调整相机角度和距离。

---

## ?? 截图指南

### 如何截图保存结果

1. **运行游戏**
2. **调用 Draw Pathfinding Debug**
3. **按 F9** 或 **Print Screen** 截图
4. 截图会保存到：
   ```
   C:/Users/你的用户名/Saved/Screenshots/Windows/
   ```

### 推荐相机角度

- **俯视角度**：查看整体路径
- **侧视角度**：查看跨层连接（3D 地下城）
- **自由视角**：详细观察特定区域

---

## ?? 完整示例

### 示例 1：基础验证

```blueprint
Event BeginPlay
    ↓
Run New Generation
    New Seed: 12345
    ↓
Delay (1 second)
    ↓
Draw Pathfinding Debug
    ├─ Generator Component: WFCGenerator
    ├─ Passable Edge Tags: [WFC.EdgeType.Road]
    ├─ Start Location: (0, 0, 0)
    ├─ End Location: (9, 9, 1)
    └─ Duration: 10.0
```

### 示例 2：持续监控

```blueprint
Event Tick (每秒触发一次)
    ↓
Branch (是否需要显示调试？)
    True:
        ↓
        Draw Pathfinding Debug
            Duration: 1.0  ← 每秒更新
```

### 示例 3：对比分析

```blueprint
// 显示约束前的结果
Run New Generation (无约束)
    ↓
Draw Pathfinding Debug
    Duration: 5.0
    ↓
Delay (5 seconds)
    ↓
// 显示约束后的结果
Run New Generation (有约束)
    ↓
Draw Pathfinding Debug
    Duration: 5.0
```

---

## ?? 总结

### 核心优势

1. **可视化直观** - 一眼看出路径是否存在
2. **实时反馈** - 无需查看日志
3. **易于调试** - 黄色球显示可到达区域
4. **灵活控制** - 可调整显示时间和内容

### 最佳实践

1. **先可视化，后诊断**
   - 用 Draw Pathfinding Debug 快速查看
   - 有问题再用 Diagnose Pathfinding Issue 详细分析

2. **合理设置 Duration**
   - 快速测试：3-5秒
   - 截图分析：10-30秒
   - 不要用太长时间（影响性能）

3. **结合其他工具**
   - `Validate Maze Path` - 验证路径
   - `Diagnose Pathfinding Issue` - 诊断问题
   - `Print All Tile Edge Tags` - 检查配置

---

## ?? 相关文档

- `PathfindingDiagnosisGuide_CN.md` - 诊断工具详细说明
- `PathfindingConstraint_EditorGuide_CN.md` - 约束配置指南
- `LargeTilesAndPathfinding_CN.md` - 大型 Tile 支持

---

**祝你调试顺利！** ????

如果可视化显示正常但约束还是不工作，请查看诊断日志进行进一步分析。
