---
name: miniengine-cpp-conventions
description: >-
  MiniEngine C++ style: d_ptr/Private pimpl, core/inc.h instead of STL headers,
  math:: over std/cmath, UTF-8 BOM+CRLF for C++. Always applies in this repo via
  .cursor/rules and AGENTS.md. Use for any Engine/GLFFViewer/Core/Render C++ work.
---

# MiniEngine C++ 约定

在 **Engine/**、**GLFFViewer/**、**Core/**、**Render/** 下写或改 C++ 时遵循本技能。

## 1. `*Private` + `d_ptr`（pimpl）

与 `PostProcessor`、`FGBufferVisualizationPass`、`World`、`GltfModel` 一致。

**公开头文件（`.h`）**

- `#include "core/inc.h"`（见第 2 节）
- 前向声明：`struct FooPrivate;`（放在 `namespace Engine` 内）
- 类末尾：`FooPrivate* d_ptr = nullptr;`
- **不要**在 `.h` 里暴露 `Private` 的成员、RHI 资源、shader 句柄等实现细节

**实现文件（`.cpp`）**

- 在本翻译单元定义 `struct FooPrivate { ... };`（通常放在匿名命名空间之上、`namespace Engine` 内）
- 构造：`d_ptr(new FooPrivate(...))`；析构：`delete d_ptr; d_ptr = nullptr;`
- 成员函数开头：`C_P(Foo);` → 使用 `d->` 访问（宏定义在 `core/inc.h`）
- `const` 成员函数参考：`C_P(const FGBufferVisualizationPass);`（与 `GBufferVisualizationPass.cpp` 一致）

**何时用**

- 新的渲染 Pass、后处理、RHI 封装、较重组件 → **默认用 pimpl**
- 纯 UI/工具类（如 `GltfViewerEditorPanel`）、无 RHI/引擎状态的薄封装 → 可不使用 `d_ptr`，但头文件仍用 `core/inc.h`

**参考**

- `Engine/Engine/Render/GBufferVisualization.h` + `Engine/Src/Engine/Render/GBufferVisualizationPass.cpp`
- `Engine/Engine/Render/PostProcessor.h` + `PostProcessor.cpp`

## 2. 头文件：优先 `core/inc.h`，不要零散 `#include <...>`

`Core/core/inc.h` 已聚合常用 STL（`string`、`vector`、`memory`、`atomic`、`filesystem` 等）及 `math/math.h`。

| 场景 | 做法 |
|------|------|
| 引擎/工具 **`.h`** | `#include "core/inc.h"`，**不要**再写 `<vector>`、`<memory>`、`<atomic>`、`<cstdint>` 等 |
| **`.cpp`** | 第一行优先 `#include "core/inc.h"`，再包含本模块头文件（与 `Actor.cpp` 一致） |
| 仅需数学类型 | `#include "math/vector3.h"` 等（其内部已含 `inc.h`） |
| 引擎类型不完整 | 补 **引擎** 头，例如 `#include "Engine/Scene/Actor.h"`，不要为 `shared_ptr<Actor>` 去猜 STL |

**反例（避免）**

```cpp
#include <atomic>
#include <memory>
#include <vector>
#include <cmath>
```

## 3. 数学：优先引擎 `math::`，不用 `<cmath>` / `std::` 三角与夹紧

`inc.h` → `math/math.h` 提供：

| 用途 | 使用 |
|------|------|
| 三角函数 | `math::Sin`、`Cos`、`Asin`、`Acos`、`Atan`、`Atan2` |
| 夹紧 / 比较 | `math::Clamp`、`math::Max`、`math::Min` |
| 常量 | `math::MATH_PI`、`math::MATH_2PI` 等 |
| 向量 | `math::Vector3`（`math/vector3.h`） |

**不要**在业务代码里写 `std::sin`、`std::cos`、`(std::max)(...)`、`#include <cmath>`，除非 `math::` 确实没有对应能力且需与第三方 API 对接。

**示例**

```cpp
pitchDeg = math::Asin(math::Clamp(c.y, -1.f, 1.f)) * (180.f / math::MATH_PI);
yawDeg = math::Atan2(c.x, c.z) * (180.f / math::MATH_PI);
dir->SetIntensity(math::Max(stren, 0.f));
```

## 4. 源文件编码（`.editorconfig`）

- **C/C++**（`.cpp`、`.h`）：**UTF-8 BOM + CRLF**（避免 MSVC C4819）
- **HLSL**：UTF-8 **无 BOM**
- 不要用脚本或批量替换破坏已有文件的 BOM/换行

## 5. 提交前自检

- [ ] 新/改动的 **`.h`** 是否用 `core/inc.h` 替代了 STL 头？
- [ ] **`.cpp`** 是否避免 `<cmath>` 等，数学是否走 `math::`？
- [ ] 重量级类是否采用 `*Private` + `d_ptr` + `C_P`？
- [ ] 使用 `Actor` 等引擎类型时是否包含完整类型头文件？
- [ ] `GetWorld()` 等返回 `shared_ptr` 的 API 是否用 `auto` / `->`，勿当成裸指针？
