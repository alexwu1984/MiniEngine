#include "ShaderUtils.hlsl"

struct VertexOutput
{
    float2 Tex : TEXCOORD;
    float4 Pos : SV_Position;
};

cbuffer BloomContants : register(b0)
{
    float BloomIntensity;
    float BloomThreshold;
    float2 BloomPad;
};

Texture2D SceneColorTexture : register(t0);
Texture2D BloomTexture : register(t1);
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

////-------------------------------------------------------
//// Simple post processing shader 
//// only tonemap and gamma
////-------------------------------------------------------
float4 PS_Tonemapping(in VertexOutput Input) : SV_Target0
{
    float3 Color = SceneColorTexture.Sample(LinearSampler, Input.Tex).xyz;
    return float4(AMDTonemapping(Color), 1.0);
}

float4 PS_ToneMapAndBloom(in VertexOutput Input) : SV_Target0
{
    float3 Color = SceneColorTexture.Sample(LinearSampler, Input.Tex).xyz;
    float3 Bloom = BloomTexture.Sample(LinearSampler, Input.Tex).xyz;
    return float4(AMDTonemapping(Color + Bloom * BloomIntensity), 1.0);
}

float4 PS_ApplyBloom(in VertexOutput Input) : SV_Target0
{
    float3 Color = SceneColorTexture.Sample(LinearSampler, Input.Tex).xyz;
    float3 Bloom = BloomTexture.Sample(LinearSampler, Input.Tex).xyz;
    return float4(Color + Bloom * BloomIntensity, 1.0);
}

Texture2D SSRBuffer : register(t1);
float4 PS_ApplySSR(in VertexOutput Input) : SV_Target0
{
    float3 Color = SceneColorTexture.Sample(LinearSampler, Input.Tex).xyz;
    float4 SSR = SSRBuffer.Sample(LinearSampler, Input.Tex);
    return float4(Color * (1-SSR.a) + SSR.rgb, 1.0);
}

static float2 offsets[9] =
{
    float2(1, 1), float2(0, 1), float2(-1, 1),
    float2(1, 0), float2(0, 0), float2(-1, 0),
    float2(1, -1), float2(0, -1), float2(-1, -1)
};

cbuffer DownSampleParam : register(b0)
{
    float2 u_invSize;
    int u_mipLevel;
    int pad;
};

////----downsample
float4 PS_DownSample(in VertexOutput Input) : SV_Target0
{
    // gaussian like downsampling
    
    float4 color = float4(0, 0, 0, 0);

    if (u_mipLevel == 0)
    {
        for (int i = 0; i < 9; i++)
            color += log(max(SceneColorTexture.Sample(LinearSampler, Input.Tex + (2 * u_invSize * offsets[i])), float4(0.01, 0.01, 0.01, 0.01)));
        return exp(color / 9.0f);
    }
    else
    {
        for (int i = 0; i < 9; i++)
            color += SceneColorTexture.SampleLevel(LinearSampler, Input.Tex + (2 * u_invSize * offsets[i]), 0);
        return color / 9.0f;
    }
}

struct BlurParam
{
    float2 u_dir;
    float2 u_resolution;
    int u_mipLevel;
    int3 pad;
};

cbuffer cbBlurParam : register(b0)
{
    BlurParam blureParam;
};

float4 blur13(Texture2D image, float2 uv, float2 resolution, float2 direction,int mipLivel) 
{
	float4 color = float4(0.0,0.0,0.0,0.0);
	float2 off1 = float2(1.411764705882353,1.411764705882353) * direction;
	float2 off2 = float2(3.2941176470588234,3.2941176470588234) * direction;
	float2 off3 = float2(5.176470588235294,5.176470588235294) * direction;
	color += image.SampleLevel(LinearSampler, uv, mipLivel) * 0.1964825501511404;
	color += image.SampleLevel(LinearSampler, uv + (off1 / resolution), mipLivel) * 0.2969069646728344;
	color += image.SampleLevel(LinearSampler, uv - (off1 / resolution), mipLivel) * 0.2969069646728344;
	color += image.SampleLevel(LinearSampler, uv + (off2 / resolution), mipLivel) * 0.09447039785044732;
	color += image.SampleLevel(LinearSampler, uv - (off2 / resolution), mipLivel) * 0.09447039785044732;
	color += image.SampleLevel(LinearSampler, uv + (off3 / resolution), mipLivel) * 0.010381362401148057;
	color += image.SampleLevel(LinearSampler, uv - (off3 / resolution), mipLivel) * 0.010381362401148057;
	return color;
}

float4 PS_Blur_old(in VertexOutput Input) : SV_Target0
{
    int s_lenght = 5;
    float s_coeffs[] = { 0.235833, 0.198063, 0.117294, 0.048968, 0.014408, }; // norm = 0.993299
    //int s_lenght = 6; float s_coeffs[] = { 0.197419, 0.174688, 0.121007, 0.065615, 0.027848, 0.009250, }; // norm = 0.994236
    //int s_lenght = 7; float s_coeffs[] = { 0.169680, 0.155018, 0.118191, 0.075202, 0.039930, 0.017692, 0.006541, }; // norm = 0.994827
    //int s_lenght = 8; float s_coeffs[] = { 0.148734, 0.138756, 0.112653, 0.079592, 0.048936, 0.026183, 0.012191, 0.004939, }; // norm = 0.995231
    //int s_lenght = 9; float s_coeffs[] = { 0.132370, 0.125285, 0.106221, 0.080669, 0.054877, 0.033440, 0.018252, 0.008924, 0.003908, }; // norm = 0.995524
    //int s_lenght = 10; float s_coeffs[] = { 0.119237, 0.114032, 0.099737, 0.079779, 0.058361, 0.039045, 0.023890, 0.013368, 0.006841, 0.003201, }; // norm = 0.995746
    //int s_lenght = 11; float s_coeffs[] = { 0.108467, 0.104534, 0.093566, 0.077782, 0.060053, 0.043061, 0.028677, 0.017737, 0.010188, 0.005435, 0.002693, }; // norm = 0.995919
    //int s_lenght = 12; float s_coeffs[] = { 0.099477, 0.096435, 0.087850, 0.075206, 0.060500, 0.045736, 0.032491, 0.021690, 0.013606, 0.008021, 0.004443, 0.002313, }; // norm = 0.996058
    //int s_lenght = 13; float s_coeffs[] = { 0.091860, 0.089459, 0.082622, 0.072368, 0.060113, 0.047355, 0.035378, 0.025065, 0.016842, 0.010732, 0.006486, 0.003717, 0.002020, }; // norm = 0.996172
    //int s_lenght = 14; float s_coeffs[] = { 0.085325, 0.083397, 0.077868, 0.069455, 0.059181, 0.048172, 0.037458, 0.027824, 0.019744, 0.013384, 0.008667, 0.005361, 0.003168, 0.001789, }; // norm = 0.996267
    //int s_lenght = 15; float s_coeffs[] = { 0.079656, 0.078085, 0.073554, 0.066578, 0.057908, 0.048399, 0.038870, 0.029997, 0.022245, 0.015852, 0.010854, 0.007142, 0.004516, 0.002743, 0.001602, }; // norm = 0.996347
    //int s_lenght = 16; float s_coeffs[] = { 0.074693, 0.073396, 0.069638, 0.063796, 0.056431, 0.048197, 0.039746, 0.031648, 0.024332, 0.018063, 0.012947, 0.008961, 0.005988, 0.003864, 0.002407, 0.001448, }; // norm = 0.996416

    float4 accum = s_coeffs[0] * SceneColorTexture.SampleLevel(LinearSampler, Input.Tex, blureParam.u_mipLevel);
    for (int i = 1; i < s_lenght; i++)
    {
        accum += s_coeffs[i] * SceneColorTexture.SampleLevel(LinearSampler, Input.Tex + blureParam.u_dir * (float) i, blureParam.u_mipLevel);
        accum += s_coeffs[i] * SceneColorTexture.SampleLevel(LinearSampler, Input.Tex - blureParam.u_dir * (float) i, blureParam.u_mipLevel);
    }

    return accum;
}

float4 PS_Blur(in VertexOutput Input) : SV_Target0
{
    float4 accum = blur13(SceneColorTexture,Input.Tex,u_resolution,u_dir,u_mipLevel);
    return accum;
}


RWTexture2D<float3> BloomResult : register(u0);

void GetSampleUV(uint2 ScreenCoord, inout float2 UV, inout float2 HalfPixelSize)
{
    float2 ScreenSize;
    BloomResult.GetDimensions(ScreenSize.x, ScreenSize.y);
    float2 InvScreenSize = rcp(ScreenSize);
    HalfPixelSize = 0.5 * InvScreenSize;
    UV = ScreenCoord * InvScreenSize + HalfPixelSize;
}

[numthreads(8, 8, 1)]
void CS_Blur(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    float2 HalfPixelSize, UV;
    GetSampleUV(DispatchThreadID.xy, UV, HalfPixelSize);

    int s_lenght = 5;
    float s_coeffs[] = { 0.235833, 0.198063, 0.117294, 0.048968, 0.014408, }; // norm = 0.993299
    
    float4 accum = s_coeffs[0] * SceneColorTexture.SampleLevel(LinearSampler, UV, blureParam.u_mipLevel);
    for (int i = 1; i < s_lenght; i++)
    {
        accum += s_coeffs[i] * SceneColorTexture.SampleLevel(LinearSampler, UV + blureParam.u_dir * (float) i, blureParam.u_mipLevel);
        accum += s_coeffs[i] * SceneColorTexture.SampleLevel(LinearSampler, UV - blureParam.u_dir * (float) i, blureParam.u_mipLevel);
    }
    BloomResult[DispatchThreadID.xy] = accum.xyz;
}

[numthreads(8, 8, 1)]
void CS_ExtractBloom(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    float2 HalfPixelSize, UV;
    GetSampleUV(DispatchThreadID.xy, UV, HalfPixelSize);

    float3 Color = SceneColorTexture.SampleLevel(LinearSampler, UV, 0).xyz;
	// clamp to avoid artifacts from exceeding fp16 through framebuffer blending of multiple very bright lights
    Color.rgb = min(float3(256 * 256, 256 * 256, 256 * 256), Color.rgb);
	
    half TotalLuminance = Luminance(Color);
    half BloomLuminance = TotalLuminance - BloomThreshold;
    half BloomAmount = saturate(BloomLuminance * 0.5f);
    BloomResult[DispatchThreadID.xy] = BloomAmount * Color;
}


[numthreads(8, 8, 1)]
void CS_DownSample(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    float2 HalfPixelSize, UV;
    GetSampleUV(DispatchThreadID.xy, UV, HalfPixelSize);

    const float Scale = 4.0f;
    float3 Result = SceneColorTexture.SampleLevel(LinearSampler, UV, 0).xyz * Scale;
    Result += SceneColorTexture.SampleLevel(LinearSampler, UV - HalfPixelSize, 0).xyz;
    Result += SceneColorTexture.SampleLevel(LinearSampler, UV + HalfPixelSize, 0).xyz;
    Result += SceneColorTexture.SampleLevel(LinearSampler, UV + float2(HalfPixelSize.x, -HalfPixelSize.y), 0).xyz;
    Result += SceneColorTexture.SampleLevel(LinearSampler, UV + float2(-HalfPixelSize.x, HalfPixelSize.y), 0).xyz;
    BloomResult[DispatchThreadID.xy] = Result / 8.0;
}

[numthreads(8, 8, 1)]
void CS_UpSample(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    float2 HalfPixelSize, UV;
    GetSampleUV(DispatchThreadID.xy, UV, HalfPixelSize);

    float3 Result = 0;
    const float Scale = 4.0f;
    Result += SceneColorTexture.SampleLevel(LinearSampler, UV + float2(-HalfPixelSize.x * Scale, 0.0), 0).xyz; //left
    Result += SceneColorTexture.SampleLevel(LinearSampler, UV + float2(+HalfPixelSize.x * Scale, 0.0), 0).xyz; //right
    Result += SceneColorTexture.SampleLevel(LinearSampler, UV + float2(0.0, -HalfPixelSize.y * Scale), 0).xyz; //up
    Result += SceneColorTexture.SampleLevel(LinearSampler, UV + float2(0.0, +HalfPixelSize.y * Scale), 0).xyz; //bottom
    Result += SceneColorTexture.SampleLevel(LinearSampler, UV + float2(-HalfPixelSize.x, -HalfPixelSize.y), 0).xyz * Scale; //top-left
    Result += SceneColorTexture.SampleLevel(LinearSampler, UV + float2(+HalfPixelSize.x, -HalfPixelSize.y), 0).xyz * Scale; //top-right
    Result += SceneColorTexture.SampleLevel(LinearSampler, UV + float2(-HalfPixelSize.x, +HalfPixelSize.y), 0).xyz * Scale; //bottom-left
    Result += SceneColorTexture.SampleLevel(LinearSampler, UV + float2(+HalfPixelSize.x, +HalfPixelSize.y), 0).xyz * Scale; //bottom-right
    BloomResult[DispatchThreadID.xy] = Result / 12.0;
}