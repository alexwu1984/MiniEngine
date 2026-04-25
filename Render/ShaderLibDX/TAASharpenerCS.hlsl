Texture2D<float4> TAABuffer : register(t0);
RWTexture2D<float4> HDR : register(u0);
//RWTexture2D<float4> History : register(u1);

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

float3 ReinhardInverse(in float3 sdr)
{
    return sdr / max(1.0f - sdr, 1e-5f);
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

    const int2 pixel = int2(globalID.xy);
    HDR[globalID.xy] = float4(TAABuffer[pixel].xyz, 1.0f);
   // History[globalID.xy] = float4(center, 1.0f);
}