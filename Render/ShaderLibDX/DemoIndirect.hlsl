/**
 * Minimal VS/PS for DemoRunner indirect-draw showcase (clip-space positions).
 */
cbuffer DemoIndirectCB : register(b0)
{
	float4 u_Color;
};

struct VSInput
{
	// Must match RHIVertexDeclare slot 0 → semantic ATTRIBUTE0 (see RHIShdader.cpp).
	float3 Position : ATTRIBUTE0;
};

struct VSOutput
{
	float4 Position : SV_POSITION;
};

VSOutput VS_Main(VSInput In)
{
	VSOutput Out;
	Out.Position = float4(In.Position, 1.0);
	return Out;
}

float4 PS_Main(VSOutput In) : SV_Target
{
	return u_Color;
}
