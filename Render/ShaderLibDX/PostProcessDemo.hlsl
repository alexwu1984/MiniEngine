#include "ShaderUtils.hlsl"

struct VertexOutput
{
    float2 Tex : TEXCOORD;
    float4 Pos : SV_Position;
};

cbuffer cbTransition1 : register(b0)
{
    int endx; // = 2
    int endy; // = -1
    float progress;
    float pad0;
};

Texture2D FromColor : register(t0);
Texture2D ToColor : register(t1);
SamplerState LinearSampler : register(s0);

VertexOutput VS_ScreenQuad(in uint VertID : SV_VertexID)
{
    VertexOutput Output;
    // Texture coordinates range [0, 2], but only [0, 1] appears on screen.
    float2 Tex = float2(uint2(VertID, VertID << 1) & 2);
    Output.Tex = Tex;
    Output.Pos = float4(lerp(float2(-1, 1), float2(1, -1), Tex), 0, 1);
    return Output;
}

bool inBounds (float2 p) 
{
    return all(float2(0.0f,0.0f) < p) && all(p < float2(1.0,1.0));
}

float Rand(float2 v) 
{
    return frac(sin(dot(v ,float2(12.9898,78.233))) * 43758.5453);
}

float2 Rotate(float2 v, float a) 
{
    float  sinval, cosval;
    sincos(a, sinval, cosval);
    //¹¹½¨Ðý×ª¾ØÕó
    float2x2 mat = float2x2(cosval, -sinval, sinval, cosval);
    return mul(v,mat);
}

float CosInterpolation(float x) 
{
    return -cos(x*PI)/2.0f+0.5f;
}

float4 transition1(float2 uv) 
{
    float2 p = uv / float2(1.0f,1.0f) - 0.5f;
    float2 rp = p;
    float rpr = (progress*2.0f-1.0f);
    float z = -(rpr*rpr*2.0f) + 3.0f;
    float az = abs(z);
    rp *= az;
    rp += lerp(float2(0.5f, 0.5f), float2(float(endx) + 0.5f, float(endy) + 0.5f), Pow2(CosInterpolation(progress)));
    float2 mrp = fmod(rp, 1.0f);
    float2 crp = rp;
    bool onEnd = int(floor(crp.x))==endx&&int(floor(crp.y))==endy;
    if(!onEnd) 
    {
        float ang = float(int(Rand(floor(crp))*4.))*.5*PI;
        mrp = float2(0.5f,0.5f) + Rotate(mrp-float2(0.5f,0.5f), ang);
    }
    if(onEnd || Rand(floor(crp))>0.5f)
    {
        return ToColor.Sample(LinearSampler, mrp); 
    } 
    else 
    {
        return FromColor.Sample(LinearSampler, mrp); 
    }
}

float4 PS_Transition1(in VertexOutput Input) : SV_Target0
{
    return transition1(Input.Tex);
}
