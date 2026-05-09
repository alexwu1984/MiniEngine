Texture2D<float4> TAABuffer : register(t0);
RWTexture2D<float4> HDR : register(u0);

float3 RGBToYCoCg(in float3 rgb)
{
    return float3(
        0.25f * rgb.r + 0.5f * rgb.g + 0.25f * rgb.b,
        0.5f * rgb.r - 0.5f * rgb.b,
        -0.25f * rgb.r + 0.5f * rgb.g - 0.25f * rgb.b);
}

float3 YCoCgToRGB(in float3 yCoCg)
{
    return float3(
        yCoCg.x + yCoCg.y - yCoCg.z,
        yCoCg.x + yCoCg.z,
        yCoCg.x - yCoCg.y - yCoCg.z);
}

float3 ApplySharpening(in float3 center, in float3 top, in float3 left, in float3 right, in float3 bottom)
{
    float3 result = RGBToYCoCg(center);
    float unsharpenMask = 4.0f * result.x;
    unsharpenMask -= RGBToYCoCg(top).x;
    unsharpenMask -= RGBToYCoCg(bottom).x;
    unsharpenMask -= RGBToYCoCg(left).x;
    unsharpenMask -= RGBToYCoCg(right).x;
    result.x = min(result.x + 0.12f * unsharpenMask, 1.05f * result.x);
    return YCoCgToRGB(result);
}

[numthreads(8, 8, 1)]
void mainCS(uint3 globalID : SV_DispatchThreadID, uint3 localID : SV_GroupThreadID, uint localIndex : SV_GroupIndex, uint3 groupID : SV_GroupID)
{
    uint width;
    uint height;
    TAABuffer.GetDimensions(width, height);
    if (globalID.x >= width || globalID.y >= height)
    {
        return;
    }

    const int xi = (int)globalID.x;
    const int yi = (int)globalID.y;
    const int w = (int)width;
    const int h = (int)height;
    const int2 p = int2(xi, yi);

    float3 center = TAABuffer[p].rgb;
    float3 top = TAABuffer[int2(xi, max(yi - 1, 0))].rgb;
    float3 bottom = TAABuffer[int2(xi, min(yi + 1, h - 1))].rgb;
    float3 left = TAABuffer[int2(max(xi - 1, 0), yi)].rgb;
    float3 right = TAABuffer[int2(min(xi + 1, w - 1), yi)].rgb;

    float3 sharpened = ApplySharpening(center, top, left, right, bottom);
    // UE r.TemporalAASharpness default ~0.5: blend toward sharpened, not full kernel (reduces ringing / shimmer).
    static const float SharpenBlend = 0.5f;
    HDR[globalID.xy] = float4(lerp(center, sharpened, SharpenBlend), 1.0f);
}
