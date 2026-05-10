// Sky radiance cubemap bake — UE 4.26 Sky Atmosphere–style *single-scattering* approximation
// (exponential Rayleigh/Mie density vs height, numerical optical depth; no LUT / multi-scatter).
// Scene convention: Y-up; observer slightly above y=0 “ground”. Matches MiniEngine bake path.
//
// Pipeline unchanged vs former ProceduralSkyCube.hlsl:
//   VS_SkyCube, cbProcSky @ b4, entry PS_ProceduralSkyCube (RHI entry name kept).

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

// PI: ShaderUtils.hlsl
static const float3 UP = float3(0.0, 1.0, 0.0);

// Earth-like scale heights (meters); UE / Bruneton family defaults.
static const float HRayleigh = 7994.0;
static const float HMie = 1200.0;
static const float AtmosphereTop = 60000.0;
static const float GroundEyOffset = 2.0;

// Sea-level extinction coefficients (1/m), RGB Rayleigh — standard literature values.
static const float3 BetaRayleighSea = float3(5.802e-6, 1.356e-5, 3.311e-5);

float RayleighPhase(float cosTheta)
{
    float c = clamp(cosTheta, -1.0, 1.0);
    float p = c * c;
    return (3.0 / (16.0 * PI)) * (1.0 + p);
}

float MiePhaseHG(float cosTheta, float g)
{
    float gg = clamp(g, -0.95, 0.95);
    float g2 = gg * gg;
    float denom = 1.0 + g2 - 2.0 * gg * cosTheta;
    denom = max(abs(denom), 1e-6);
    float inv = 1.0 / pow(denom, 1.5);
    return (1.0 / (4.0 * PI)) * ((1.0 - g2) * inv);
}

float HeightAboveGround(float3 p)
{
    return max(0.0, dot(p, UP));
}

// Length along `dir` (normalized) from `origin` until height reaches AtmosphereTop or ground (y<=0), whichever first.
float ViewRayAtmosphereLength(float3 origin, float3 dir)
{
    float3 d = normalize(dir);
    float oy = origin.y;
    float dy = d.y;

    float tMax = AtmosphereTop * 4.0;

    if (dy > 1e-5)
    {
        float tUp = (AtmosphereTop - oy) / dy;
        if (tUp > 0.0)
            tMax = min(tMax, tUp);
    }
    else if (dy < -1e-5)
    {
        float tGround = -oy / dy;
        if (tGround > 0.0)
            tMax = min(tMax, tGround);
    }

    return max(tMax, 0.0);
}

// Optical depth ∫ σ ds along segment [origin, origin+dir*len], dir normalized.
float3 IntegrateOpticalDepth(float3 origin, float3 dir, float len, int steps, float3 betaRSea, float3 betaMSea)
{
    if (len <= 0.0)
        return float3(0.0, 0.0, 0.0);
    float ds = len / float(steps);
    float3 accum = float3(0.0, 0.0, 0.0);
    for (int i = 0; i < steps; ++i)
    {
        float t = (float(i) + 0.5) * ds;
        float3 p = origin + dir * t;
        float h = HeightAboveGround(p);
        float rhoR = exp(-h / HRayleigh);
        float rhoM = exp(-h / HMie);
        float3 sigma = betaRSea * rhoR + betaMSea * rhoM;
        accum += sigma * ds;
    }
    return accum;
}

float4 PS_ProceduralSkyCube(VertexOutput In) : SV_Target
{
    float3 sunDir = normalize(vSunDirection);
    float3 viewDir = normalize(SkyCubeDirectionFromHClip(In.HClip));

    // Do not force black below the horizon: that fills the cubemap’s lower hemisphere and makes
    // the sky pass + IBL sample pure black past the horizon (harsh line / dead ground haze).
    // Downward rays hit the ground plane quickly; the same scattering integral applies.

    float3 eye = float3(0.0, GroundEyOffset, 0.0);

    // Match legacy knob ranges: default rayleigh=2, turbidity=10 in C++.
    float3 betaR = BetaRayleighSea * max(rayleigh, 1e-4) * 0.5;
    float mieScale = max(mieCoefficient, 1e-6) * max(turbidity, 1.0) * 1e-6;
    float3 betaM = float3(21e-6, 21e-6, 21e-6) * mieScale * 400.0;

    float g = clamp(mieDirectionalG, -0.95, 0.95);

    float lenV = ViewRayAtmosphereLength(eye, viewDir);
    const int kViewSteps = 14;
    const int kSunSteps = 8;

    float dsV = lenV / float(kViewSteps);
    float3 odViewSegEnd = float3(0.0, 0.0, 0.0);
    float3 scatterRGB = float3(0.0, 0.0, 0.0);

    float cosTheta = dot(viewDir, sunDir);
    float phaseR = RayleighPhase(cosTheta);
    float phaseM = MiePhaseHG(cosTheta, g);

    // UE-style illuminance scale for HDR cubemap (tunable via `luminance`).
    float3 sunIlluminance = float3(1.0, 1.0, 1.0) * (110000.0 * max(luminance, 0.0));

    for (int iv = 0; iv < kViewSteps; ++iv)
    {
        float tv = (float(iv) + 0.5) * dsV;
        float3 p = eye + viewDir * tv;
        float h = HeightAboveGround(p);
        float rhoR = exp(-h / HRayleigh);
        float rhoM = exp(-h / HMie);
        float3 sigmaS_R = betaR * rhoR;
        float3 sigmaS_M = betaM * rhoM;
        float3 sigmaExt = sigmaS_R + sigmaS_M;

        float3 odViewToSample = odViewSegEnd + 0.5 * sigmaExt * dsV;

        float lenSun = ViewRayAtmosphereLength(p, sunDir);
        float3 opticalSun = IntegrateOpticalDepth(p, sunDir, lenSun, kSunSteps, betaR, betaM);

        float3 tau = odViewToSample + opticalSun;
        tau = min(tau, float3(80.0, 80.0, 80.0));
        float3 transmittance = exp(-tau);

        float3 sr = sigmaS_R * phaseR;
        float3 sm = sigmaS_M * phaseM;
        scatterRGB += (sr + sm) * sunIlluminance * transmittance * dsV;

        odViewSegEnd += sigmaExt * dsV;
    }

    // ACES-ish compression similar to previous procedural sky (stable HDR for mips).
    float3 Lin = scatterRGB * 8.5e-5;
    Lin = max(Lin, float3(0.0, 0.0, 0.0));

    float sunAngularDiameterCos = 0.999956676946448443553574619906976478926848692873900859324;
    float3 transZenith = exp(-IntegrateOpticalDepth(eye, sunDir, ViewRayAtmosphereLength(eye, sunDir), kSunSteps, betaR, betaM));
    float sundisk = smoothstep(sunAngularDiameterCos, sunAngularDiameterCos + 0.00006, cosTheta);
    float3 L0 = transZenith * sundisk * float3(1.0, 0.98, 0.92) * (45000.0 * max(luminance, 0.0));

    float3 texColor = Lin + L0 * 5e-5;
    texColor += float3(0.0, 0.00015, 0.00035);

    // Single scattering collapses when the sun hugs the horizon; the baked cubemap feeds both the sky pass
    // and diffuse IBL — without fill, the lower hemisphere and grazing directions stay near black while the
    // motorcycle scene looks fine only after raising the sun (more energy everywhere).
    float sunHt = max(sunDir.y, 1e-4);
    float lowSun = pow(saturate((0.52 - sunHt) / 0.52), 1.15);
    float horizBand = saturate(1.0 - abs(viewDir.y) * 2.35);
    float belowEye = saturate(-viewDir.y * 3.5);
    float fillAmt = max(luminance, 0.0) * lowSun * (0.055 + 0.42 * horizBand + 0.28 * belowEye);
    float3 fillTint = lerp(float3(0.22, 0.28, 0.38), float3(0.42, 0.34, 0.26), saturate(1.0 - sunHt * 1.2));
    texColor += fillTint * fillAmt;

    float lum = dot(texColor, float3(0.2126, 0.7152, 0.0722));
    texColor /= (1.0 + lum * 0.032);

    return float4(texColor, 1.0);
}
