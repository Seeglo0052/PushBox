# WFC 寻路约束文档索引

## ?? 文档列表

本目录包含 WFC 寻路验证约束的完整文档。根据你的需求选择合适的文档：

---

## ?? 我想快速开始 → 查看这个

### ? [编辑器使用指南（中文）](PathfindingConstraint_EditorGuide_CN.md)
**推荐！最完整的傻瓜式教程**

内容：
- ? 3步快速配置
- ? 图文详细说明
- ? 参数完整解释
- ? Tile 配置教程
- ? 故障排查指南
- ? 配置模板

**适合**：所有用户，尤其是新手

---

## ?? 我需要快速参考 → 查看这个

### ?? [快速参考卡（中文）](QuickReference_CN.md)
**快速查找参数和配置**

内容：
- 3步设置流程
- 常用配置模板
- 参数速查表
- Tile 标签示例
- 故障排查清单
- 性能推荐表

**适合**：已经会用，需要快速查找参数

---

## ?? 我想了解技术细节 → 查看这些

### ?? [实现总结（中文）](PathfindingImplementationSummary_CN.md)
**完整的技术说明和原理**

内容：
- 工作原理详解
- A* 算法实现
- RandomStream 兼容性说明
- 性能优化建议
- 网络复制机制
- 技术架构

**适合**：想深入理解实现原理的开发者

### ?? [最终总结（中文）](FinalSummary_CN.md)
**功能总览和使用流程**

内容：
- 功能特性列表
- 使用场景说明
- 配置参数详解
- 调试技巧
- 性能建议
- FAQ

**适合**：想全面了解功能的用户

---

## ?? 我想看代码示例 → 查看这些

### ?? [使用示例（中文）](PathfindingUsageExamples.cpp)
**6个实际代码示例**

内容：
- 示例1：简单 2D 迷宫
- 示例2：3D 地下城
- 示例3：使用 Blueprint 库
- 示例4：多人游戏
- 示例5：生成后验证
- 示例6：Tile 配置

**适合**：需要 C++ 代码参考的开发者

---

## ?? 我想看完整API文档 → 查看这个

### ?? [完整文档（英文）](PathfindingConstraint.md)
**完整的API参考和使用指南**

内容：
- 详细API说明
- 算法实现细节
- 使用示例
- 性能考虑
- 故障排除
- FAQ

**适合**：需要完整英文文档的开发者

---

## ?? 快速决策树

```
我是新手，想快速上手
    ↓
    查看：编辑器使用指南（中文）?
    
我已经会用，需要查参数
    ↓
    查看：快速参考卡（中文）
    
我想理解技术原理
    ↓
    查看：实现总结（中文）
    
我需要代码示例
    ↓
    查看：使用示例（中文）
    
我想看完整的英文文档
    ↓
    查看：完整文档（英文）
```

---

## ?? 文件结构

```
Documentation/
├── PathfindingConstraint_EditorGuide_CN.md  ? 推荐新手看
├── QuickReference_CN.md                     ?? 快速参考
├── PathfindingImplementationSummary_CN.md   ?? 技术细节
├── FinalSummary_CN.md                       ?? 功能总结
├── PathfindingUsageExamples.cpp             ?? 代码示例
├── PathfindingConstraint.md                 ?? 英文完整文档
└── README_Documentation.md                  ?? 本文件
```

---

## ?? 推荐阅读顺序

### 对于新用户：
1. ?? 先看：**编辑器使用指南**（了解如何配置）
2. ?? 然后看：**代码示例**（如果需要动态添加）
3. ?? 最后：**快速参考卡**（日常使用）

### 对于有经验的开发者：
1. ?? 先看：**实现总结**（理解技术原理）
2. ?? 然后看：**最终总结**（了解功能特性）
3. ?? 最后：**代码示例**（实际应用）

---

## ?? 学习路径

### 初级：快速上手
```
编辑器使用指南 → 快速参考卡
```
时间：15-30分钟
目标：能够在编辑器中配置约束

### 中级：深入理解
```
最终总结 → 实现总结 → 代码示例
```
时间：1-2小时
目标：理解原理并能在代码中使用

### 高级：完全掌握
```
所有文档 + 源代码阅读
```
时间：3-4小时
目标：能够修改和扩展功能

---

## ? 常见问题跳转

| 问题 | 查看文档 | 章节 |
|------|---------|------|
| 如何在编辑器中配置？ | 编辑器使用指南 | 第1-2步 |
| 如何配置 Tile 标签？ | 编辑器使用指南 | 配置 Tile 资产 |
| 参数怎么设置？ | 快速参考卡 | 参数速查 |
| 约束不起作用？ | 快速参考卡 | 故障排查 |
| 如何动态添加约束？ | 代码示例 | 示例3 |
| 多人游戏如何使用？ | 代码示例 | 示例4 |
| 如何验证路径？ | 代码示例 | 示例5 |
| 技术原理是什么？ | 实现总结 | 工作原理 |
| RandomStream 兼容吗？ | 实现总结 | RandomStream 兼容性 |
| 性能如何优化？ | 最终总结 | 性能建议 |

---

## ?? 相关资源

### 源代码位置
- 头文件：`Plugins/WFC/Source/WFC/Public/Core/Constraints/WFCPathfindingConstraint.h`
- 实现：`Plugins/WFC/Source/WFC/Private/Core/Constraints/WFCPathfindingConstraint.cpp`
- Blueprint库：`Plugins/WFC/Source/WFC/Public/WFCPathfindingBlueprintLibrary.h`

### 相关约束类参考
- `WFCBoundaryConstraint`：边界约束
- `WFCCountConstraint`：数量约束
- `WFCArcConsistencyConstraint`：邻接一致性约束

---

## ?? 获取帮助

### 遇到问题？按顺序检查：

1. **查看快速参考卡** → 故障排查清单
2. **查看编辑器使用指南** → 常见问题
3. **查看日志输出** → Output Log 中的 LogWFC 信息
4. **查看代码示例** → 确认使用方式正确

### 日志关键字

在 Output Log 中搜索：
- `PathfindingConstraint`：约束相关信息
- `Valid path found`：找到有效路径
- `No valid path`：路径验证失败

---

## ?? 开始使用

推荐从 **[编辑器使用指南](PathfindingConstraint_EditorGuide_CN.md)** 开始！

只需3步，无需写代码，就能让你的迷宫保证可解！

祝你使用愉快！??
