#include "PostProcess.hlsl"

cbuffer ConstantBuffer : register(b0)
{
    float u_a;
    float u_b;
    float u_c;
    float u_d;
    float u_fPower;
    float u_noise;
    float u_glowWeight;
    float u_glowBias;
    float u_glowEdge0;
    float u_glowEdge1;
    float2 rectHalfSize;
    float3 u_midPoint;
    float pad0;
    float2 u_quadNDC2ScreenNDCScale;
    float2 pad1;
};

float sdRectangle(float2 p, float2 halfSize)
{
    float2 d = abs(p) - halfSize;
    float outsideDist = length(max(d, 0.0f));
    float insideDist = min(max(d.x, d.y), 0.0f);
    // 带符号距离：外部为正，内部为负，边界为0
    return outsideDist + insideDist;
}

float f(float x)
{
    return 1.0f - u_b * pow(u_c * exp(1.0f), -u_d * x - u_a);
}

// 辅助函数：随机噪声
float rand(float2 co)
{
    return frac(sin(dot(co, float2(12.9898f, 78.233f))) * 43758.5453f);
}

// 辅助函数：发光效果
float Glow(float2 texCoord)
{
    return sin(atan2(texCoord.y * 2.0f - 1.0f, texCoord.x * 2.0f - 1.0f) - 0.5f);
}

float4 PS_LiquidGlass(in VertexOutput Input) : SV_Target0
{
    float2 center = float2(0.5f, 0.5f);
    float2 p = (Input.Tex- center) * 2.0f; // 归一化到[-1,1]
    
    // 计算矩形距离（使用常量缓冲区的rectHalfSize）
    float d = sdRectangle(p, rectHalfSize);
    
    // 裁剪矩形外部像素
    if (d > 0.0f)
        discard;
    
    // 距离映射（与原逻辑一致）
    float dist = -d;
    float2 sampleP = p * pow(f(dist), u_fPower);
    
    float2 targetNDC = sampleP * u_quadNDC2ScreenNDCScale + u_midPoint.xy;
    float2 coord = targetNDC * 0.5f + float2(0.5f, 0.5f);
    
   // float2 coord = (Input.Tex + sampleP*0.1) * 0.5f + float2(0.5f, 0.5f);
    
    // 边界检查（超出范围返回品红）
    coord = clamp(coord, 0.0f, 1.0f);
    
    // 噪声计算
    float noise = rand(Input.Pos.xy * 1e-3f) - 0.5f;
    
    // 纹理采样（HLSL需显式指定采样器）
    float4 color = SceneColorTexture.Sample(LinearSampler, coord) + float4(noise * u_noise, noise * u_noise, noise * u_noise, 0.0f);
    
    // 发光效果计算
    float mul = Glow(Input.Tex) * u_glowWeight * smoothstep(u_glowEdge0, u_glowEdge1, dist) + 1.0f + u_glowBias;
    
    return color * float4(mul, mul, mul, 1.0f);
}