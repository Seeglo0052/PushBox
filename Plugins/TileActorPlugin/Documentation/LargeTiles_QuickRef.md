# 大型 Tile 配置快速参考

## ?? 一句话总结

**大型 Tile 内部边缘留空，系统自动识别为可通过。**

---

## ? 正确配置示例

### 斜坡 Tile (1×1×2)

```
底层:
  入口: WFC.EdgeType.Road  ?
  内部: <Empty>            ? (自动可通过)
  地基: WFC.EdgeType.Pillar

顶层:
  内部: <Empty>            ? (自动可通过)
  出口: WFC.EdgeType.Road  ?
```

---

## ? 错误配置示例

### 错误 1：给内部边缘设置了标签

```
底层:
  +Z: WFC.EdgeType.Interior  ? (除非添加到 Passable Tags)

正确做法:
  +Z: <Empty>                ? (自动可通过)
```

### 错误 2：所有边缘都是空的

```
所有边缘: <Empty>  ? (无法区分内外)

正确做法:
  外部: WFC.EdgeType.Wall/Door  ?
  内部: <Empty>                  ?
```

---

## ?? 验证方法

### 查看诊断日志

**正确**：
```
+Z: <Empty/Interior> [OK]  ?
```

**错误**：
```
+Z: WFC.EdgeType.Something [X]  ?
```

---

## ?? 记住

1. 空标签 = 内部 = 可通过
2. 无需配置 Passable Edge Tags
3. 只设置外部边缘的标签
4. 让系统自动处理内部连通性

---

## ?? 详细文档

- `LargeTilesAndPathfinding_CN.md` - 完整技术说明
- `PathfindingConstraint_EditorGuide_CN.md` - 配置指南
