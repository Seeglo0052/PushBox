# 寻路可视化 - 快速参考

## ?? 一行总结

**在3D世界中用彩色线和球体显示寻路结果。**

---

## ?? 最简单的用法

```
Run New Generation
    ↓
Delay (1 second)
    ↓
Draw Pathfinding Debug
    ├─ Generator Component: WFCGenerator
    ├─ Passable Edge Tags: [你的标签]
    ├─ Start Location: (0, 0, 0)
    ├─ End Location: (9, 9, 1)
    └─ Duration: 10.0
```

---

## ?? 颜色含义

| 颜色 | 含义 | 说明 |
|------|------|------|
| ?? 绿色大球 | 起点 | 50单位半径 |
| ?? 红色大球 | 终点 | 50单位半径 |
| ?? 蓝色线 | 路径 | 连接路径上的单元格 |
| ?? 青色小球 | 路径上的点 | 30单位半径 |
| ?? 黄色小球 | 可到达区域 | 从起点能到达的所有位置 |
| ? 红色 X | 找不到路径 | 在起点和终点中间显示 |

---

## ?? 关键参数

```
Duration: 10.0      ← 显示10秒
Draw Path: ?        ← 绘制路径（如果找到）
Draw Grid: ?        ← 不绘制网格（大网格时避免杂乱）
```

---

## ?? 一眼诊断

### 找到路径 ?
```
?? → ?? → ?? → ?? → ??
     "PATH FOUND! Length: XX"
```

### 找不到路径 ?
```
??     ?     ??
??????
"NO PATH FOUND!"
```

**黄色球** = 从起点能到达的区域

---

## ?? 快速修复

### 没有黄色球（起点被困）
→ 检查起点 Tile 的边缘标签

### 黄色球很多但不到终点
→ 中间有阻碍，检查 Passable Edge Tags

### 路径很长/绕远
→ 某些 Tile 配置问题或缺少直连 Tile

---

## ?? 截图

按 **F9** 或 **Print Screen** 截图保存

---

## ?? 详细文档

`PathfindingVisualization_CN.md` - 完整使用指南
