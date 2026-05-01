// Kajiya-Kay hair/fur shading (strand tangent-based diffuse + specular).

#ifndef HAIRSHADING_HLSL
#define HAIRSHADING_HLSL

float3 DecodeHairTangentOctPacked(float2 enc01)
{
	float2 f = enc01 * 2.0 - 1.0;
	float3 v = float3(f.xy, 1.0 - dot(abs(f.xy), float2(1.0, 1.0)));
	if (v.z < 0.0)
	{
		float2 sx = float2(f.x >= 0.0 ? 1.0 : -1.0, f.y >= 0.0 ? 1.0 : -1.0);
		v.xy = (1.0 - abs(v.yx)) * sx;
	}
	return normalize(v);
}

float2 EncodeHairTangentOctPacked(float3 dir)
{
	float3 v = normalize(dir);
	float invL1 = dot(abs(v), float3(1.0, 1.0, 1.0));
	if (invL1 > 1e-6)
		v /= invL1;
	float2 oct = v.xy;
	if (v.z < 0.0)
	{
		float2 sx = float2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
		oct.xy = (1.0 - abs(v.yx)) * sx;
	}
	return oct * 0.5 + 0.5;
}

void KajiyaKayTerms(float3 strandT, float3 L, float3 V, float perceptualRoughness, float3 baseColor,
	out float3 diffuseTerm, out float3 specTerm)
{
	float3 H = normalize(L + V);
	float TL = dot(strandT, L);
	float TV = dot(strandT, V);
	float TH = dot(strandT, H);
	float sinTL = sqrt(saturate(1.0 - TL * TL));
	float sinTV = sqrt(saturate(1.0 - TV * TV));
	float sinTH = sqrt(saturate(1.0 - TH * TH));

	float kd = sinTL * sinTV;
	// Small floor so cylinders aren’t black off the principal planes; keeps fur readable vs flat BRDF.
	diffuseTerm = baseColor * saturate(kd * 0.9 + 0.14);

	float rough = saturate(perceptualRoughness);
	float specPower = lerp(72.0, 8.0, rough);
	float specIntensity = pow(max(sinTH, 1e-5), specPower);
	float3 specColor = float3(0.07, 0.07, 0.07);
	specTerm = specColor * specIntensity * 1.35;
}

#endif // HAIRSHADING_HLSL
