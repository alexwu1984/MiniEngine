# MiniEngine 阴影绘制流程说明

本文描述 **Shadow RDG Pass** 从场景数据收集、深度贴图生成，到延迟光照中 **PCSS 采样** 的完整链路。帧级总览见 [`SceneRenderer-Frame-Graph.md`](SceneRenderer-Frame-Graph.md)；光照计算（Cluster、全屏 Deferred、前向半透明）见 [`Lighting-Rendering.md`](Lighting-Rendering.md)。

---

## 1. 相关源码

| 模块 | 路径 |
|------|------|
| RDG 调度 | `Engine/Src/Engine/Render/SceneRendering/SceneRenderer.cpp` → Pass `"Shadow"` |
| 阴影 Pass 入口 | `Engine/Src/Engine/Render/Shadow/ShadowRenderPass.cpp` |
| 视图数据聚合 | `Engine/Src/Engine/Render/Shadow/FShadowViewData.cpp` |
| 平行光 depth | `Engine/Src/Engine/Render/Shadow/FDirectionalShadowDepthPass.cpp` |
| 视锥拟合 | `Engine/Src/Engine/Render/Shadow/FDirectionalShadowFrustumFitter.cpp` |
| 点光立方体 | `Engine/Src/Engine/Render/Shadow/FPointShadowCubePass.cpp` |
| 聚光 depth | `Engine/Src/Engine/Render/Shadow/FSpotShadowDepthPass.cpp` |
| Mesh 绘制 | `Engine/Src/Engine/Render/Shadow/FShadowDepthMeshDrawer.cpp` |
| 单 Mesh VS/PS | `Engine/Src/Engine/Render/Shadow/FShadowPassMeshDraw.cpp` |
| 深度 VS/PS | `Render/ShaderLibDX/ShadowPass-VS.hlsl`, `ShadowPass-PS.hlsl` |
| 延迟采样 | `Render/ShaderLibDX/DirectionalShadow.hlsl`, `ShadowPCSS.hlsl`, `SpotShadowSampling.hlsl` |
| 延迟绑定 | `Engine/Src/Engine/Render/SceneRendering/DeferredLightingPass.cpp` |
| 场景快照 | `Engine/Engine/Render/Shadow/ShadowProjectorTypes.h` → `FShadowProjectorSceneData` |

---

## 2. 阴影在帧管线中的位置

阴影 Pass 在 **Skylight 更新之后、清空 GBuffer 之前** 执行，保证后续 BasePass / DeferredLighting 能绑定已生成的 depth 比较采样纹理。

```mermaid
flowchart TB
  subgraph before ["Shadow 之前"]
    SKY[UpdateSkyLightCaptures]
  end

  subgraph shadow ["Shadow Pass"]
    SH[ShadowRenderPass::Render]
    DIR[平行光 depth 2048² / CSM 竖向 Atlas]
    PT[点光 Cube 6 面 × 512²]
    SP[聚光 depth 2048²]
  end

  subgraph after ["Shadow 之后"]
    CL[ClearSceneTextures]
    BP[RenderBasePass]
    DL[DeferredLighting<br/>绑定 DirectionalShadow / PointShadowCube / SpotShadow]
    PP[后处理 …]
    SDW[ShadowDebugWire]
  end

  SKY --> SH
  SH --> DIR
  SH --> PT
  SH --> SP
  DIR --> CL
  PT --> CL
  SP --> CL
  CL --> BP --> DL --> PP --> SDW
```

**RDG 依赖**（`SceneRenderer`）：`Shadow` → `ClearSceneTextures` / `RenderBasePass` / `DeferredLighting` / …（见帧图文档 §5）。

---

## 3. 游戏线程 → 渲染包（输入侧）

```mermaid
flowchart LR
  subgraph gt ["游戏线程"]
    W[World / Scene]
    PSC[ProjShadow 组件<br/>GatherShadowCasters]
    RCV[可见接收体<br/>GatherShadowFrustumBounds]
    LGT[LightsForShadow<br/>ShadowMapIndex ≥ 0]
    PROJ[BuildShadowProjectorAggregateData<br/>FShadowProjectorSceneData]
    CAM[主相机 Near/Far/Forward<br/>写入 ProjectorScene]
  end

  subgraph pkt ["FSceneRenderPacket"]
    SC[ShadowCasters]
    FB[ShadowFrustumBounds]
    SL[LightsForShadow]
    PS[ShadowProjectorScene]
  end

  W --> PSC --> SC
  W --> RCV --> FB
  W --> LGT --> SL
  W --> PROJ --> PS
  CAM --> PS
  SC --> pkt
  FB --> pkt
  SL --> pkt
  PS --> pkt
```

| 列表 | 用途 |
|------|------|
| `ShadowCasters` | 仅 **ProjShadow** 标记的 Actor 网格，**实际写入** depth 贴图 |
| `ShadowFrustumBounds` | 可见接收体（如地面）并集 AABB，用于 **平行光/聚光视锥** 拟合，避免大平面缺影 |
| `LightsForShadow` | 本帧参与阴影的光源副本；`ShadowMapIndex` 决定点光/聚光槽位 |
| `FShadowProjectorSceneData` | 相机 ze 范围、CSM 开关与 split、可选 ProjShadow 模型 AABB、主视图世界包围盒（收紧平行光 XY） |

**是否调度 Shadow Pass**（`SceneRenderer`）：

```text
bScheduleShadowPass =
  ShadowCasters 非空
  || ShadowFrustumBounds 非空
  || ShadowProjectorScene.bValid
  || 任意 Light.ShadowMapIndex >= 0
```

若不调度，则 `InvalidateCachedMainLightForShading()`，延迟光照使用上一帧无效化后的 fallback 深度纹理。

---

## 4. ShadowRenderPass::Render 总流程

```mermaid
flowchart TB
  START([ShadowRenderPass::Render])

  START --> PRUNE[MeshDrawer::PruneStaleMeshShadowPasses]
  PRUNE --> BUILD[FShadowViewData::Build]

  BUILD --> SLOTS{LightSlots}
  SLOTS --> D{DirectionalLightListIndex ≥ 0<br/>且 bSubjectValid?}
  D -->|是| ATLAS[按需重建 DepthRenderBuffer<br/>CSM: 2048×N 竖向 Atlas]
  ATLAS --> DIRPASS[FDirectionalShadowDepthPass::Render]
  DIRPASS --> CACHE_DIR[缓存 CachedDirectionalShadow<br/>CachedMainLightForShading]

  SLOTS --> P{PointCubeShadowLightListIndex ≥ 0<br/>且有 caster?}
  P -->|是| PT_PASS[FPointShadowCubePass::Render<br/>6 面循环]
  PT_PASS --> CACHE_PT[缓存 FaceVP / PosRange]

  SLOTS --> S{SpotShadowLightListIndex ≥ 0?}
  S -->|是| SP_PASS[FSpotShadowDepthPass::Render]
  SP_PASS --> CACHE_SP[缓存 SpotLightViewProj]

  CACHE_DIR --> VP[恢复 Viewport 为 depth RT 全尺寸]
  CACHE_PT --> VP
  CACHE_SP --> VP
  VP --> END([返回])

  D -->|否| P
  P -->|否| S
  S -->|否| VP
```

---

## 5. FShadowViewData::Build（单帧阴影上下文）

```mermaid
flowchart TB
  IN[ShadowCasters + FrustumBounds + Lights + ProjectorScene]

  IN --> SEL[SelectShadowSubjectMeshListForFrustum<br/>caster 优先 / 或 receiver 列表]
  SEL --> SUBJ[BuildMergedShadowSubjectWorldAabb]
  IN --> RECV[BuildMergedShadowReceiverWorldAabb<br/>FrustumBoundsMeshes]

  IN --> FIND_D[FindFirstDirectionalLightIndex]
  IN --> FIND_P[FindPointShadowCubeLightIndex<br/>ShadowMapIndex == 2]
  IN --> FIND_S[FindSpotShadowLightIndex<br/>ShadowMapIndex == 3]

  SUBJ --> OUT[FShadowViewData]
  RECV --> OUT
  FIND_D --> OUT
  FIND_P --> OUT
  FIND_S --> OUT
```

---

## 6. 平行光深度（FDirectionalShadowDepthPass）

### 6.1 单贴图 vs CSM 竖向 Atlas

| 模式 | RT 尺寸 | Viewport | CB `cbDirectionalShadow` |
|------|---------|----------|---------------------------|
| 非 CSM | 2048 × 2048 | 全图 | `DirectionalCSMEnabled=0`, `CascadeCount=1`, `CascadeViewProj[0]` |
| CSM（2–3 级） | 2048 × (2048×N) | 每级 `vpY = ci × 2048` | `DirectionalCSMEnabled=1`, `CascadeSplits`（ze 分界）, `CascadeViewProj[0..2]` |

`bDirectionalShadowCSM` 与 `DirectionalShadowCSMCascadeCount` 来自 `FShadowProjectorSceneData`（场景/世界设置）。

### 6.2 非 CSM 流程

```mermaid
flowchart TB
  A[取主平行光 Light] --> B[SetupDirectionalShadowViewProjection<br/>Subject + Receiver AABB]
  B --> C[写 CachedDirectionalShadow<br/>单 CascadeViewProj]
  C --> D[Clear depth → 1.0]
  D --> E[Viewport 2048×2048]
  E --> F[MeshDrawer::DrawDirectional<br/>所有 ShadowCaster]
```

### 6.3 CSM 流程（每级 Cascade）

```mermaid
flowchart TB
  SPLIT[FillLinearZeSplitEnds<br/>CameraNear/Far + Split0/1 归一化]
  SPLIT --> LOOP{ci = 0 .. CascadeCount-1}

  LOOP --> SUB[ComputeCascadeSubjectWorldAabb<br/>相机 ze 切片 ∩ 投射体 AABB]
  SUB --> FIT[SetupDirectionalShadowViewProjection<br/>传入 cascadeSubject 裁剪 mesh 光空间范围]
  FIT --> VP[SetViewPort 0, ci×2048, 2048, 2048]
  VP --> DRAW[DrawDirectional 同一批 caster]
  DRAW --> LOOP

  LOOP --> CB[汇总 CascadeViewProj + CascadeSplits<br/>CameraForwardInvCount.w = 1/CascadeCount]
```

**视锥拟合要点**（`FDirectionalShadowFrustumFitter`）：

- 正交投影包围 **Subject**（投射体）与 **Receiver**（接收体 XY，低角度阳光时与主视图包围盒求交）。
- **Texel snap**：平移 `LightView` 使阴影稳定，减少相机平移时的阴影游泳。
- CSM 每级必须用 **该级 cascade subject**，否则全场景 mesh 合并会导致各级 ortho 相同、depth 重复。

### 6.4 平行光视锥拟合（概念）

```mermaid
flowchart LR
  subgraph inputs ["输入"]
    SUB[SubjectWorldAabb]
    REC[ReceiverWorldAabb]
    CAM[CameraForward / ViewBounds]
    MESH[SubjectMeshList 光空间合并]
  end

  subgraph fit ["SetupDirectionalShadowViewProjection"]
    ORTHO[正交 LightView + Proj]
    SNAP[SnapLightViewTranslationToShadowTexels]
    XY[可选 ExpandOrthoXY from receivers]
  end

  SUB --> ORTHO
  REC --> XY
  CAM --> XY
  MESH --> ORTHO
  ORTHO --> SNAP
  SNAP --> VP[LightViewProj 写入 Light + CB]
```

---

## 7. 点光立方体阴影（FPointShadowCubePass）

```mermaid
flowchart TB
  FIND[首个 Point + ShadowMapIndex==2] --> CLEAR[对 Cube 每个 face Clear depth]
  CLEAR --> FACE{face 0..5}
  FACE --> VP[ComputePointShadowFaceViewProj<br/>90° FOV, zNear=0.05, zFar=Range]
  VP --> DRAW[MeshDrawer::DrawCubeFace]
  DRAW --> FACE
  FACE --> CACHE[CachedPointFaceVP[6] + LightPosRange]
```

- 资源：`PF_ShadowDepth` Cube，**512×512×6**。
- 延迟阶段：`CBPointShadow` + `PointShadowCube` SRV，在光照循环中对 `LightIndex` 匹配的点光做立方体 PCF。

---

## 8. 聚光阴影（FSpotShadowDepthPass）

```mermaid
flowchart TB
  FIND[首个 Spot + ShadowMapIndex==3]
  FIND --> BOUNDS[Subject ∪ Receiver AABB → 扩展 zFar]
  BOUNDS --> SETUP[SetupSpotShadowViewProjection<br/>LookAt 沿 -Direction, FOV from OuterCone]
  SETUP --> CLR[Clear SpotShadowBuffer 2048²]
  CLR --> DRAW[MeshDrawer::DrawDirectional<br/>注意：复用平行光绘制路径，仅 VP 不同]
  DRAW --> CACHE[CachedSpotLightViewProj + LightView]
```

- 无 caster 时可用 `FrustumBoundsMeshes` 仅驱动 zFar（仍可能绘制空 depth）。
- 延迟：`CBSpotShadow` + `SpotShadow` 2D 比较深度 + `SpotShadowSampling.hlsl`。

---

## 9. 深度 Mesh 绘制（FShadowDepthMeshDrawer / FShadowPassMeshDraw）

```mermaid
flowchart TB
  subgraph per_mesh ["每个 ShadowCaster Mesh"]
    FILTER{MeshWritesShadowMapDepth?<br/>不透明 或 透明且有 BaseColor 贴图}
    FILTER -->|否| SKIP[跳过]
    FILTER -->|是| CACHE[PassMeshDraws map 查找/创建 FShadowPassMeshDraw]
    CACHE --> SKIN[UpdatePassMeshDrawPalette<br/>蒙皮骨骼矩阵]
    SKIN --> DRAW[FShadowPassMeshDraw::Draw / DrawCubeFace]
  end

  subgraph gpu ["GPU"]
    VS[ShadowPass-VS<br/>LightViewProj × World]
    PS[ShadowPass-PS<br/>可选 AlphaClip]
    OM[仅 Depth RT<br/>PF_ShadowDepth 比较采样]
  end

  DRAW --> VS --> PS --> OM
```

| 规则 | 说明 |
|------|------|
| `MeshWritesShadowMapDepth` | 不透明材质写 depth；透明需有 albedo 纹理才 clip 写影 |
| `kMatShaderFlag_ShadowAlphaClip` | PS 对 albedo.a 做 `clip` |
| 缓存 | 按 `MeshBase*` 缓存 VS/IL；场景切换时 `ClearCachedMeshShadowPasses` |

---

## 10. 延迟光照消费（采样阶段）

Shadow Pass **只生成 depth**；可见性在 **DeferredLighting / Forward** 的 PS 中计算。

```mermaid
flowchart TB
  subgraph shadow_pass ["Shadow Pass 输出"]
    D2D[DirectionalShadow 2D RT<br/>可能 2048×6144 Atlas]
    CUBE[PointShadowCube]
    SPOT[SpotShadow 2D]
    CB7[cbDirectionalShadow b7]
    CBPT[CBPointShadow]
    CBSP[CBSpotShadow]
  end

  subgraph deferred ["DeferredLightingPass"]
    FILL[FillPerFrameFromView<br/>Lights + ShadowMapIndex 校验]
    BIND[RDG 纹理: DirectionalShadow / SpotShadow / PointShadowCube]
    PS[DeferredLighting PS]
  end

  subgraph hlsl ["Shader"]
    CSM[DirectionalShadowVisibility<br/>按 ze 选 cascade]
    PCSS[ComputeShadowPCSSAtlasTile<br/>ShadowPCSS.hlsl]
    PT[点光立方体采样]
    SP[SpotShadowSampling]
  end

  D2D --> BIND
  CUBE --> BIND
  SPOT --> BIND
  CB7 --> FILL
  FILL --> PS
  BIND --> PS
  PS --> CSM --> PCSS
  PS --> PT
  PS --> SP
```

**平行光 CSM 采样**（`DirectionalShadow.hlsl`）：

1. `ze = dot(worldPos - camPos, CameraForward)`。
2. 与 `CascadeSplits` 比较得到 `idx`（0..CascadeCount-1）。
3. `world → CascadeViewProj[idx] → NDC → Atlas 行 idx`（`CameraForwardInvCount.w` 为每行高度比例）。
4. `ComputeShadowPCSSAtlasTile` 做 PCSS（块搜索 + 滤波）。

主平行光 `Light.ShadowMapIndex == 0` 且存在有效 `GetShadowMap()` 时才保留阴影；否则置 `-1` 并绑定 fallback 比较深度纹理。

---

## 11. 资源与常量一览

| 资源 | 格式 | 分辨率 | ShadowMapIndex |
|------|------|--------|----------------|
| 平行光 depth | `PF_ShadowDepth` RT | 2048² 或 2048×(2048×N) | 0（主平行光） |
| 点光 cube | `PF_ShadowDepth` Cube | 512²×6 | 2（`kPointLightCubeShadowMapIndex`） |
| 聚光 depth | `PF_ShadowDepth` RT | 2048² | 3（`kSpotLightShadowMapIndex`） |

**Clear 值**：深度清为 **1.0**（远平面），与 D3D 深度比较一致。

---

## 12. 调试与失效

```mermaid
flowchart LR
  SH[Shadow Pass 结束] --> DBG[CollectShadowDebugLightShapes]
  DBG --> WIRE[ShadowDebugWire Pass<br/>Tonemap 之后]
  CSM_DBG[UpdateDirectionalCSMCascadeSubjectDebugFromShadowPass<br/>CSM 各级 Subject AABB 线框]
  SH --> CSM_DBG
```

- 场景切换：`FlushClearShadowPassMeshCacheNow` → `InvalidateCachedMainLightForShading` + `ClearCachedMeshShadowPasses`。
- CSM Atlas 尺寸变化会 **重建** `DepthRenderBuffer` 并清空 Mesh 阴影 Pass 缓存。

---

## 13. 端到端数据流（一图总览）

```mermaid
sequenceDiagram
  participant GT as 游戏线程
  participant SR as SceneRenderer
  participant SRP as ShadowRenderPass
  participant MD as FShadowDepthMeshDrawer
  participant DL as DeferredLighting

  GT->>SR: Packet(ShadowCasters, FrustumBounds, Lights, ProjectorScene)
  SR->>SRP: RDG Pass Shadow
  SRP->>SRP: FShadowViewData::Build
  alt 平行光
    SRP->>SRP: FrustumFitter + DirectionalDepthPass
    SRP->>MD: DrawDirectional → depth Atlas/2D
  end
  alt 点光
    SRP->>MD: DrawCubeFace × 6
  end
  alt 聚光
    SRP->>MD: DrawDirectional(spot VP)
  end
  SR->>DL: ClearSceneTextures … DeferredLighting
  DL->>DL: 绑定 depth SRV + 填充 CB
  DL->>DL: DirectionalShadowVisibility / PCSS / Spot / Point
```

---

## 14. 与半透明 / Fur 的关系

- **半透明前向**、**Fur** 在 Shadow Pass **之后** 执行，不参与 depth 贴图生成。
- 半透明/Fur 光照可读 `cbDirectionalShadow` 与主平行光 PCSS（与延迟共用 `DirectionalShadow.hlsl` 思路）。
- 运动矢量 MRT 与阴影 depth **独立**；TAA 不直接依赖阴影贴图。

---

*文档版本与代码一致点：Shadow Pass 在 `ClearSceneTextures` 之前；平行光 CSM 为竖向 Atlas；每帧每种局部光型各占用一个 shadow 槽（首个匹配光源）。*
