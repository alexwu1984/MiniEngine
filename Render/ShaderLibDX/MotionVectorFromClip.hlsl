#ifndef MOTION_VECTOR_FROM_CLIP_HLSL
#define MOTION_VECTOR_FROM_CLIP_HLSL

#include "PerFrameStruct.hlsl"

#ifndef MINIENGINE_CALCULATE_3D_VELOCITY_DEFINED
#define MINIENGINE_CALCULATE_3D_VELOCITY_DEFINED

// Single source: unjittered NDC delta for TAA velocity buffer (opaque / translucent / fur).
float3 Calculate3DVelocity(float4 CurrentClip, float4 PrevClip)
{
	const float Wc = CurrentClip.w != 0.0 ? CurrentClip.w : 1e-8;
	const float Wp = PrevClip.w != 0.0 ? PrevClip.w : 1e-8;
	const float2 ScreenPos = CurrentClip.xy / Wc - myPerFrame.TemporalAAJitter.xy;
	const float2 PrevScreenPos = PrevClip.xy / Wp - myPerFrame.TemporalAAJitter.zw;
	return float3(ScreenPos - PrevScreenPos, CurrentClip.z / Wc - PrevClip.z / Wp);
}

#endif // MINIENGINE_CALCULATE_3D_VELOCITY_DEFINED

#endif // MOTION_VECTOR_FROM_CLIP_HLSL
