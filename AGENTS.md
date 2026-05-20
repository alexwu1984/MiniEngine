# MiniEngine — Agent 说明

在本仓库中编写或修改 C++（Engine、GLFFViewer、Core、Render）时，**始终**遵循项目 C++ 约定。

## 必读

| 资源 | 路径 |
|------|------|
| 始终生效规则 | `.cursor/rules/miniengine-cpp-conventions.mdc` |
| 完整技能文档 | `.cursor/skills/miniengine-cpp-conventions/SKILL.md` |

## 要点（摘要）

1. **pimpl**：新 Pass / 重组件默认 `*Private` + `d_ptr` + `C_P`（见 `FGBufferVisualizationPass`、`PostProcessor`）。
2. **头文件**：优先 `core/inc.h`，避免零散 STL 头。
3. **数学**：优先 `math::`（`math/math.h`），不用 `<cmath>` / `std::` 三角与夹紧。
4. **编码**：C++ UTF-8 BOM + CRLF；HLSL UTF-8 无 BOM。

细节、反例与提交前清单见技能文件。
