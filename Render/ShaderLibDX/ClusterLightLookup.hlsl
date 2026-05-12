#ifndef CLUSTER_LIGHT_LOOKUP_HLSL
#define CLUSTER_LIGHT_LOOKUP_HLSL

// Clustered Forward+ pass-2 helper: maps the rasterized pixel to a cluster index and exposes the per-cluster
// (offset, count) plus the flat light index list written by `ClusterLightBuildCS.hlsl`. Forward translucent + fur
// shaders include this AFTER PerFrameStruct.hlsl + their material includes so the SRV registers line up.
//
// Bindings (PS SRV table — keep in sync with DeferredLightingPass.cpp constants):
//   t13 : StructuredBuffer<Light>     _SceneLights              (already declared by the including file)
//   t14 : StructuredBuffer<uint2>     _ClusterLightOffsetCount
//   t15 : StructuredBuffer<uint>      _ClusterLightIndexList
//
// Grid constants must match CLUSTER_GRID_* in ClusterLightBuildCS.hlsl and Engine::ClusterLightCulling::*.
#define CLUSTER_GRID_X 24
#define CLUSTER_GRID_Y 12
#define CLUSTER_GRID_Z 24
#define CLUSTER_COUNT (CLUSTER_GRID_X * CLUSTER_GRID_Y * CLUSTER_GRID_Z)
#define MAX_LIGHTS_PER_CLUSTER 64

StructuredBuffer<uint2> _ClusterLightOffsetCount : register(t14);
StructuredBuffer<uint> _ClusterLightIndexList : register(t15);

// Computes the cluster index for the rasterized pixel.
//   svPos.xy = screen-space pixel position (top-left origin, half-pixel offset).
//   svPos.w  = 1 / clip_w = 1 / view_z for the standard perspective projection used by the engine.
// Z slicing is logarithmic to match the CS; the slot 0 starts at CameraNearZ and slot (CLUSTER_GRID_Z-1) ends at CameraFarZ.
uint ClusterIndexFromPixel(float4 svPos)
{
	const float viewZ = (svPos.w > 1e-6) ? (1.0 / svPos.w) : myPerFrame.CameraNearZ;
	const float safeNear = max(myPerFrame.CameraNearZ, 1e-3);
	const float safeFar = max(myPerFrame.CameraFarZ, safeNear + 1e-3);

	// Pixel -> cluster XY: InvScreenResolution = 1 / (width, height), so pixel.xy * InvScreenResolution gives [0,1].
	uint cx = (uint)clamp(floor(svPos.x * myPerFrame.InvScreenResolution.x * (float)CLUSTER_GRID_X), 0.0, (float)(CLUSTER_GRID_X - 1));
	uint cy = (uint)clamp(floor(svPos.y * myPerFrame.InvScreenResolution.y * (float)CLUSTER_GRID_Y), 0.0, (float)(CLUSTER_GRID_Y - 1));

	// Pixels closer than near (rare, hit by partial sub-pixel sampling) map to slot 0; beyond far -> last slot.
	const float zRatio = max(viewZ, safeNear) / safeNear;
	const float czFloat = log(zRatio) / log(safeFar / safeNear) * (float)CLUSTER_GRID_Z;
	uint cz = (uint)clamp(floor(czFloat), 0.0, (float)(CLUSTER_GRID_Z - 1));

	return cx + cy * (uint)CLUSTER_GRID_X + cz * (uint)(CLUSTER_GRID_X * CLUSTER_GRID_Y);
}

#endif // CLUSTER_LIGHT_LOOKUP_HLSL
