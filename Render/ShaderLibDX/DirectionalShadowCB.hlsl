// Shared cbDirectionalShadow (b7) for deferred / fur / translucency. Match Engine::CBDirectionalShadow in MaterialPreFrame.h.
#ifndef MINIENGINE_DIRECTIONAL_SHADOW_CB_HLSL
#define MINIENGINE_DIRECTIONAL_SHADOW_CB_HLSL

cbuffer cbDirectionalShadow : register(b7)
{
	row_major matrix CascadeViewProj[3];
	float4 CascadeSplits;
	float4 CameraForwardInvCount;
	int DirectionalCSMEnabled;
	int CascadeCount;
	int2 _PadDirectionalShadow;
};

#endif // MINIENGINE_DIRECTIONAL_SHADOW_CB_HLSL
