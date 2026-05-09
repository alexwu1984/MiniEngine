// Procedural sky cubemap capture — Preetham / Cauldron SkyDomeProc math.
// glTFSample applies extra log2/pow after texColor; we store linear HDR for IBL and preview with ACES only.
// Same raw Lin+L0 as desktop SkyDomeProc collapses to one white continent — damp forward scatter + sun peak here.

#include "ShaderUtils.hlsl"
#include "PerFrameStruct.hlsl"

struct VertexIN
{
    float3 Position : ATTRIBUTE0;
};

struct VertexOutput
{
    float4 Position : SV_Position;
    float3 LocalDirection : TEXCOORD0;
    float4 HClip : TEXCOORD1;
};

VertexOutput VS_SkyCube(VertexIN In)
{
    VertexOutput Out;
    Out.LocalDirection = In.Position;
    float4 h = mul(mul(float4(In.Position, 1.0), GetWorldMatrix()), GetCameraViewProj());
    Out.HClip = h;
    Out.Position = h;
    return Out;
}

cbuffer cbProcSky : register(b4)
{
    float3 vSunDirection;
    float  padding0;
    float  rayleigh;
    float  turbidity;
    float  mieCoefficient;
    float  luminance;
    float  mieDirectionalG;
    float3 _pad1;
};

static const float e = 2.71828182845904523536028747135266249775724709369995957;
static const float3 lambda = float3(680E-9, 550E-9, 450E-9);
static const float3 totalRayleigh = float3(5.804542996261093E-6, 1.3562911419845635E-5, 3.0265902468824876E-5);
static const float v = 4.0;
static const float3 K = float3(0.686, 0.678, 0.666);
static const float3 MieConst = float3(1.8399918514433978E14, 2.7798023919660528E14, 4.0790479543861094E14);
static const float cutoffAngle = 1.6110731556870734;
static const float steepness = 1.5;
static const float EE = 1000.0;

static const float3 cameraPos = float3(0.0, 0.0, 0.0);
static const float pi = 3.141592653589793238462643383279502884197169;
static const float rayleighZenithLength = 8.4E3;
static const float mieZenithLength = 1.25E3;
static const float3 up = float3(0.0, 1.0, 0.0);
static const float sunAngularDiameterCos = 0.999956676946448443553574619906976478926848692873900859324;
static const float THREE_OVER_SIXTEENPI = 0.05968310365946075;
static const float ONE_OVER_FOURPI = 0.07957747154594767;

float sunIntensity(float zenithAngleCos)
{
    zenithAngleCos = clamp(zenithAngleCos, -1.0, 1.0);
    return EE * max(0.0, 1.0 - pow(e, -((cutoffAngle - acos(zenithAngleCos)) / steepness)));
}

float3 totalMie(float T)
{
    float c = (0.2 * T) * 10E-18;
    return 0.434 * c * MieConst;
}

float rayleighPhase(float cosTheta)
{
    return THREE_OVER_SIXTEENPI * (1.0 + pow(cosTheta, 2.0));
}

float hgPhase(float cosTheta, float g)
{
    float g2 = pow(g, 2.0);
    float inverse = 1.0 / pow(abs(1.0 - 2.0 * g * cosTheta + g2), 1.5);
    return ONE_OVER_FOURPI * ((1.0 - g2) * inverse);
}

float4 PS_ProceduralSkyCube(VertexOutput In) : SV_Target
{
    float3 vWorldPosition = SkyCubeDirectionFromHClip(In.HClip);
    float3 sunDir = normalize(vSunDirection);

    float vSunE = sunIntensity(dot(sunDir, up));
    float vSunfade = 1.0 - clamp(1.0 - exp((sunDir.y)), 0.0, 1.0);
    float rayleighCoefficient = rayleigh - (1.0 * (1.0 - vSunfade));

    float3 vBetaR = totalRayleigh * rayleighCoefficient;
    float3 vBetaM = totalMie(turbidity) * mieCoefficient;

    float3 viewDir = normalize(vWorldPosition - cameraPos);
    // SkyDomeProc.hlsl: acos(max(0, dot(up, dir))) — cutoff at horizon for optical length integral.
    float zenithAngle = acos(max(0.0, dot(up, viewDir)));
    float inverse = 1.0 / (cos(zenithAngle) + 0.15 * pow(abs(93.885 - ((zenithAngle * 180.0) / pi)), -1.253));
    float sR = rayleighZenithLength * inverse;
    float sM = mieZenithLength * inverse;

    float3 Fex = exp(-(vBetaR * sR + vBetaM * sM));

    float cosTheta = dot(viewDir, sunDir);
    float rPhase = rayleighPhase(cosTheta * 0.5 + 0.5);
    float3 betaRTheta = vBetaR * rPhase;
    float mPhase = hgPhase(cosTheta, mieDirectionalG);
    float3 betaMTheta = vBetaM * mPhase;

    float3 scatterInner = vSunE * ((betaRTheta + betaMTheta) / (vBetaR + vBetaM)) * (1.0 - Fex);
    float3 Lin = pow(abs(scatterInner), float3(1.1, 1.1, 1.1));
    Lin *= lerp(float3(1.0, 1.0, 1.0),
                pow(vSunE * ((betaRTheta + betaMTheta) / (vBetaR + vBetaM)) * Fex, float3(0.5, 0.5, 0.5)),
                clamp(pow(1.0 - dot(up, sunDir), 5.0), 0.0, 1.0));

    // Tame forward Mie without carving a dark annulus: crushing Lin to ~0.05 while sundisk is a narrow smoothstep
    // leaves a ring where both terms are tiny → black halo around the sun.
    float cs = saturate(cosTheta);
    float fwd = smoothstep(0.86, 0.9994, cs);
    fwd *= fwd;
    Lin *= lerp(1.0, 0.48, fwd);

    float3 L0 = float3(0.1, 0.1, 0.1) * Fex;
    float sundisk = smoothstep(sunAngularDiameterCos, sunAngularDiameterCos + 0.000038, cosTheta);
    float sunEScale = min(vSunE, 420.0);
    L0 += (sunEScale * 92.0 * Fex) * sundisk;

    float3 texColor = (Lin + L0) * 0.04 + float3(0.0, 0.0003, 0.00075);
    texColor *= max(luminance, 0.0);
    float lum = dot(texColor, float3(0.2126, 0.7152, 0.0722));
    texColor /= (1.0 + lum * 0.032);

    return float4(texColor, 1.0);
}
