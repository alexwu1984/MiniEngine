# MiniEngine

基于 DirectX 11 / 12 的学习型实时渲染引擎（Windows + CMake + MSVC）。主程序 **GLFFViewer** 加载 GLB 场景并走完整延迟着色管线；**DemoRunner** 提供独立示例；**SoftwareRender** 为软件光栅参考实现。

更细的 RDG、纹理池与屏障说明见 [doc/RDG-RHI-Resource-And-Barriers.md](doc/RDG-RHI-Resource-And-Barriers.md)。

---

## 核心库（Core）

- **数学**：`vector2/3/4`、`matrix4x4`、`quaternion`、`plane3`、`aabb3`、`ray3`、`frustum`（视锥裁剪）
- **基础**：命令行、日志、计时、Win32 窗口与输入等

---

## 资源与场景

- **模型**：glTF 2.0 / **GLB**（tinygltf）；可选 **Assimp** 网格导入
- **动画**：骨骼动画通道、BlendShape；GPU **骨骼蒙皮**（`Skinning.hlsl`）
- **物理骨骼**：**Dynamic Bone**（质点链 + 弹簧约束的骨骼 secondary motion，非屏幕粒子特效）
- **材质**：PBR（金属/粗糙度、法线、自发光等）；**Fur** 壳层材质 + **Kajiya-Kay** 头发光照（延迟光照后的前向 Fur pass）

---

## 渲染

### 主路径（延迟 + 前向补充）

- **GBuffer Base Pass** → **延迟光照**（PBR BRDF）
- **Cluster Forward+**：Compute 构建 per-cluster 光源列表（`ClusterLightBuildCS`）
- **半透明**：前向 PBR（`TranslucentPBRForward`）
- **阴影**
  - 平行光：**CSM**（级联阴影贴图）
  - 点光源：立方体 shadow map
  - 聚光灯：2D shadow map
- **环境光**：**SkyLight / IBL**（HDR 或程序化天空 → Cubemap、漫反射/镜面预滤波、Split hemisphere 等）
- **调试**：阴影与光源范围的线框叠加（`ShadowDebugWire`）

### 后处理（`FRDGBuilder` 帧图）

- **TAA**、**FXAA**
- **SSR**（屏幕空间反射）
- **Bloom**（多级降采样/合成；D3D12 上 Bloom 中间链可走 **RDG 瞬态 UAV + Aliasing 堆**）
- **Tonemapping**
- 通用 **Blur**（PS / CS，多用于其他 pass）

跨帧 **`RenderTexturePool`** 复用后处理中间纹理；D3D12 另提供 **Transient Aliasing Pool**（帧末裁剪空闲 heap）。

### RHI 与着色器

- **DynamicRHI**：**D3D11**、**D3D12** 双后端
- **FRDG**：Pass 登记、拓扑编译、Pass-begin 资源屏障（D3D12 按 `ERDGResourceAccess` 推断；D3D11 barrier 为 no-op）
- 离线 **Shader 预编译**（`.cso`）+ 运行时 **JIT**（`ShaderLibDX`）
- **ImGui** 调试与 Demo 面板

---

## 可执行目标

| 目标 | 说明 |
|------|------|
| `GLFFViewer` | 主查看器：加载 GLB、完整场景渲染 |
| `DemoRunner` | 独立 Demo（如后处理、SkyLight IBL 等） |
| `SoftwareRender` | CPU 软件渲染参考 |
| `CompileShaderBlob` | 离线编译 HLSL 工具 |

---

## 构建（简要）

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

运行前需将 `ShaderLibDX`、模型等资源同步到可执行文件目录（CMake `POST_BUILD` 已配置 `sync_shaderlib_dx` 与资源拷贝）。具体选项见根目录 `CMakeLists.txt`。
