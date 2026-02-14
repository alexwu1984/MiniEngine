#include "ShaderUtils.hlsl"

Texture2D GBufferA		: register(t0); // normal
Texture2D GBufferB		: register(t1); // MetallicSpecularRoughness
Texture2D SceneDepthZ	: register(t2); // Depth
Texture2D HistorySceneColor	: register(t3); // history scene buffer (for reprojecting hit points)
Texture2D VelocityBuffer	: register(t4); // velocity buffer
Texture2D SSRHistoryBuffer	: register(t5); // SSR history buffer (for temporal accumulation denoising)

SamplerState LinearSampler	: register(s0);
SamplerState PointSampler	: register(s1);

cbuffer PSContant : register(b0)
{
	float4x4 ViewProj;
	float4x4 InvViewProj;
	float3	CameraPos;
	float WorldThickness;
	int NumRays;
	int FrameIndex; // Random seed for temporal dimension (replaces FrameIndexMod8)
    float2 Resolution;
	float TemporalBlendFactor; // Temporal blend factor (0-1), higher = smoother but slower response
    float3 Pad0;
};

float3 ProjectWorldPos(float3 WorldPos)
{
	float4 ClipPos = mul(float4(WorldPos, 1.0), ViewProj);
	float3 Projected = ClipPos.xyz / ClipPos.w;
	Projected.xy = Projected.xy * float2(0.5, -0.5) + 0.5; //[-1,1] -> [0,1]
	return Projected;
}

// unproject a screen point to world space
float3 UnprojectScreen(float3 ScreenPoint)
{
	ScreenPoint.xy = float2(2.0, -2.0) * ScreenPoint.xy + float2(-1.0, 1.0); //[0,1] -> [-1,1]
	float4 WorldPos = mul(float4(ScreenPoint, 1.0), InvViewProj);
	return WorldPos.xyz / WorldPos.w;
}

float3 IntersectDepthPlane(float3 RayOrigin, float3 RayDir, float t)
{
	return RayOrigin + RayDir * t;
}

bool CastSimpleRay(float3 Start, float3 Direction, float ScreenDistance, out float3 OutHitUVz)
{
	float PerPixelThickness = ScreenDistance;
	float PerPixelCompareBias = 0.85 * PerPixelThickness;

	float2 TextureSize;
	SceneDepthZ.GetDimensions(TextureSize.x, TextureSize.y);
	int MaxLinearStep = max(TextureSize.x, TextureSize.y);

	Direction = normalize(Direction);
	float3 Step = Direction;
	float StepScale = abs(Direction.x) > abs(Direction.y) ? TextureSize.x : TextureSize.y;
	Step /= StepScale;

	float Depth;
	float3 Ray = Start;
	for (int i = 0; i < MaxLinearStep; ++i)
	{
		Ray += Step;
		if (Ray.z < 0 || Ray.z > 1)
			return false;
		Depth = SceneDepthZ.SampleLevel(PointSampler, Ray.xy, 0).x;
		if (Depth + PerPixelCompareBias < Ray.z && Ray.z < Depth + PerPixelThickness)
		{
			OutHitUVz = Ray;
			return true;
		}
	}
	return false;
}

float ComputeHitVignetteFromScreenPos(float2 ScreenPos)
{
	float2 Vignette = saturate(abs(ScreenPos) * 5 - 4);
	return saturate(1.0 - dot(Vignette, Vignette));
}

void ReprojectHit(float3 HitUVz, out float2 OutPrevUV, out float OutVignette)
{
	float2 ThisScreen = 2.0 * HitUVz.xy - 1.0; //[-1,1]
	float4 ThisClip = float4(ThisScreen, HitUVz.z, 1);

	float2 Velocity = VelocityBuffer.SampleLevel(PointSampler, HitUVz.xy, 0).xy;
	float2 PrevScreen = ThisClip.xy - Velocity;

	float2 PrevUV = 0.5 * PrevScreen.xy + 0.5;
	OutVignette = min(ComputeHitVignetteFromScreenPos(ThisScreen), ComputeHitVignetteFromScreenPos(PrevScreen));
	OutPrevUV = PrevUV;
}

float4 PS_SSR(float2 Tex : TEXCOORD, float4 SVPosition : SV_Position) : SV_Target
{
	float3 N = GBufferA.Sample(LinearSampler, Tex).xyz;
	N = 2.0 * N - 1.0;

	float Depth = SceneDepthZ.SampleLevel(LinearSampler, Tex, 0).x;

	float3 Screen0 = float3(Tex, Depth);
	float3 World0 = UnprojectScreen(Screen0);

	float3 V = normalize(CameraPos - World0);

	float3 MetallicSpecularRoughness = GBufferB.SampleLevel(LinearSampler, Tex, 0).xyz;
	float Roughness = MetallicSpecularRoughness.z;
	float a = Roughness * Roughness;
	float a2 = a * a;

	uint2 PixelPos = (uint2)SVPosition.xy;
	uint2 Random = Rand3DPCG16(int3(PixelPos, FrameIndex)).xy;

	float3x3 TangentBasis = GetTangentBasis(N);
	float3 TangentV = mul(TangentBasis, V);

	float4 CurrentFrameColor = 0;
	int HitCount = 0;
	for (int i = 0; i < NumRays; i++)
	{
		float2 E = Hammersley16(i, NumRays, Random);
		float3 H = mul(ImportanceSampleVisibleGGX(UniformSampleDisk(E), a2, TangentV).xyz, TangentBasis);
		float3 L = 2 * dot(V, H) * H - V;
		//float3 L = reflect(-V, N);

		float3 World1 = World0 + L * WorldThickness;
		float3 Screen1 = ProjectWorldPos(World1);

		float ScreenDistance = abs(Screen1.z - Screen0.z);
		float3 StartScreen = Screen0;			//[0, 1]
		float3 StepScreen = Screen1 - Screen0;	//[-1, 1]

		float3 HitUVz;
		bool bHit = CastSimpleRay(StartScreen, StepScreen, ScreenDistance, HitUVz);
		if(bHit)
		{
			float Vignette;
			float2 PrevUV;
			ReprojectHit(HitUVz, PrevUV, Vignette);
			CurrentFrameColor += HistorySceneColor.SampleLevel(LinearSampler, PrevUV, 0) * Vignette;
			HitCount++;
		}
	}

	// Average current frame sampling results
	if (HitCount > 0)
	{
		CurrentFrameColor /= HitCount;
	}

	// Temporal accumulation denoising: blend current and history frames
	float4 FinalColor = CurrentFrameColor;
	if (FrameIndex > 1 && HitCount > 0)
	{
		// Use motion vector to reproject history frame SSR result
		float2 Velocity = VelocityBuffer.SampleLevel(PointSampler, Tex, 0).xy;
		float2 HistoryUV = Tex - Velocity;
		
		// Check if history UV is within valid range
		bool bValidHistory = all(HistoryUV >= 0.0) && all(HistoryUV <= 1.0);
		
		if (bValidHistory)
		{
			float4 HistoryColor = SSRHistoryBuffer.SampleLevel(LinearSampler, HistoryUV, 0);
			
			// Calculate blend weight: confidence based on velocity
			float VelocityMagnitude = length(Velocity * Resolution);
			float VelocityConfidence = saturate(1.0 - VelocityMagnitude / 10.0); // Higher velocity = lower history weight
			
			// Color difference detection: if current and history frames differ too much, reduce history weight
			float ColorDiff = length(CurrentFrameColor.rgb - HistoryColor.rgb);
			float ColorDiffThreshold = 0.1;
			float ColorDiffFactor = saturate(1.0 - ColorDiff / ColorDiffThreshold);
			
			// Combined blend factor
			float BlendWeight = TemporalBlendFactor * VelocityConfidence * ColorDiffFactor;
			BlendWeight = saturate(BlendWeight);
			
			// Blend current frame and history frame
			FinalColor = lerp(CurrentFrameColor, HistoryColor, BlendWeight);
		}
	}

	return FinalColor;
}
