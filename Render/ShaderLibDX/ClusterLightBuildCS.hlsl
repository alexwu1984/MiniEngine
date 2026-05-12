// Clustered Forward+ pass-1: for each of the 24*12*24 view-frustum clusters, list the analytic lights
// (point / spot / directional) that touch its view-space AABB. Output is consumed in the same frame by
// `TranslucentPBRForward.hlsl` and `FurForwardAccumulate.hlsl`.
//
// Cluster decomposition:
//   - XY  : screen-space tile grid (CLUSTER_GRID_X * CLUSTER_GRID_Y).
//   - Z   : logarithmic depth slices between camera near and far (z[i] = near * (far/near)^(i / CLUSTER_GRID_Z)).
// Each thread (one per cluster) reconstructs the view-space AABB from the 4 NDC tile corners projected onto the
// cluster's near and far depth planes via the inverse projection matrix, then sphere-tests every entry in
// `_SceneLights`. Directional lights bypass the test (they affect every cluster). The writes are register-only
// (no atomics) because each cluster has its own contiguous slot in `_ClusterLightIndexList`.
//
// Padding decisions:
//   - MAX_LIGHTS_PER_CLUSTER bounds the per-cluster overflow so the index list stays a fixed-size buffer.
//   - The list buffer is allocated as CLUSTER_COUNT * MAX_LIGHTS_PER_CLUSTER uints regardless of scene light count;
//     small enough at 24*12*24 * 64 = ~1.7 MB to keep things simple before any compaction is needed.

#include "PerFrameStruct.hlsl"

#define CLUSTER_GRID_X 24
#define CLUSTER_GRID_Y 12
#define CLUSTER_GRID_Z 24
#define CLUSTER_COUNT (CLUSTER_GRID_X * CLUSTER_GRID_Y * CLUSTER_GRID_Z)
#define MAX_LIGHTS_PER_CLUSTER 64

cbuffer cbClusterBuild : register(b0)
{
	// View / projection split out from cbPerFrame so the cluster CS can do its own NDC <-> view-space conversions
	// without depending on the rasterizer's combined matrices (which include TAA jitter).
	row_major matrix ClusterViewMatrix;
	row_major matrix ClusterInvProjMatrix;
	float ClusterNearZ;
	float ClusterFarZ;
	uint ClusterLightCount;
	uint ClusterPad0;
};

StructuredBuffer<Light> _SceneLightsCS : register(t0);
RWStructuredBuffer<uint2> _ClusterLightOffsetCount : register(u0);
RWStructuredBuffer<uint> _ClusterLightIndexList : register(u1);

float3 NDCRayDirView(float2 ndcXY)
{
	// Map NDC (z = 0, w = 1) through the inverse projection -> view-space ray hitting the near plane.
	float4 v = mul(float4(ndcXY, 0.0, 1.0), ClusterInvProjMatrix);
	return v.xyz / max(v.w, 1e-6);
}

float3 ViewPointAtZ(float3 ray, float zView)
{
	// Ray origin is camera (0,0,0) in view-space; scale so its z component lands on the cluster's depth plane.
	return ray * (zView / max(ray.z, 1e-5));
}

[numthreads(64, 1, 1)]
void MainCS(uint3 dispatchId : SV_DispatchThreadID)
{
	uint clusterIndex = dispatchId.x;
	if (clusterIndex >= (uint)CLUSTER_COUNT)
		return;

	const uint cx = clusterIndex % (uint)CLUSTER_GRID_X;
	const uint cy = (clusterIndex / (uint)CLUSTER_GRID_X) % (uint)CLUSTER_GRID_Y;
	const uint cz = clusterIndex / ((uint)CLUSTER_GRID_X * (uint)CLUSTER_GRID_Y);

	// Logarithmic depth slice for this cluster; clamp inputs so log() never sees <= 0 even if the CB carries garbage.
	const float SafeNear = max(ClusterNearZ, 1e-3);
	const float SafeFar = max(ClusterFarZ, SafeNear + 1e-3);
	const float LogRatio = log(SafeFar / SafeNear);
	const float zNear = SafeNear * exp(LogRatio * (float)cz / (float)CLUSTER_GRID_Z);
	const float zFar = SafeNear * exp(LogRatio * (float)(cz + 1u) / (float)CLUSTER_GRID_Z);

	const float invGx = 1.0 / (float)CLUSTER_GRID_X;
	const float invGy = 1.0 / (float)CLUSTER_GRID_Y;
	const float xMinNDC = ((float)cx * invGx) * 2.0 - 1.0;
	const float xMaxNDC = ((float)(cx + 1u) * invGx) * 2.0 - 1.0;
	// NDC Y is inverted relative to screen Y; cluster (cy=0) is the top row.
	const float yMaxNDC = 1.0 - ((float)cy * invGy) * 2.0;
	const float yMinNDC = 1.0 - ((float)(cy + 1u) * invGy) * 2.0;

	const float3 r00 = NDCRayDirView(float2(xMinNDC, yMinNDC));
	const float3 r01 = NDCRayDirView(float2(xMinNDC, yMaxNDC));
	const float3 r10 = NDCRayDirView(float2(xMaxNDC, yMinNDC));
	const float3 r11 = NDCRayDirView(float2(xMaxNDC, yMaxNDC));

	float3 corners[8];
	corners[0] = ViewPointAtZ(r00, zNear);
	corners[1] = ViewPointAtZ(r01, zNear);
	corners[2] = ViewPointAtZ(r10, zNear);
	corners[3] = ViewPointAtZ(r11, zNear);
	corners[4] = ViewPointAtZ(r00, zFar);
	corners[5] = ViewPointAtZ(r01, zFar);
	corners[6] = ViewPointAtZ(r10, zFar);
	corners[7] = ViewPointAtZ(r11, zFar);

	float3 aabbMin = corners[0];
	float3 aabbMax = corners[0];
	[unroll]
	for (int k = 1; k < 8; ++k)
	{
		aabbMin = min(aabbMin, corners[k]);
		aabbMax = max(aabbMax, corners[k]);
	}

	const uint writeOffset = clusterIndex * (uint)MAX_LIGHTS_PER_CLUSTER;
	uint count = 0u;

	[loop]
	for (uint i = 0u; i < ClusterLightCount && count < (uint)MAX_LIGHTS_PER_CLUSTER; ++i)
	{
		Light light = _SceneLightsCS[i];
		bool include = false;
		if (light.Type == LightType_Directional)
		{
			include = true;
		}
		else
		{
			// Sphere-AABB intersection in view space. Unlimited-range lights (Range < 0) are admitted unconditionally
			// — same convention `GetRangeAttenuation` already uses on the PS side.
			float3 posView = mul(float4(light.Position, 1.0), ClusterViewMatrix).xyz;
			if (light.Range < 0.0)
			{
				include = true;
			}
			else
			{
				float3 closest = clamp(posView, aabbMin, aabbMax);
				float3 d = posView - closest;
				const float radius = max(light.Range, 1e-4);
				include = dot(d, d) < radius * radius;
			}
		}

		if (include)
		{
			_ClusterLightIndexList[writeOffset + count] = i;
			++count;
		}
	}

	_ClusterLightOffsetCount[clusterIndex] = uint2(writeOffset, count);
}
