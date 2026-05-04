#pragma once
#include "core/inc.h"
#include "math/matrix4x4.h"
#include "math/vector2.h"
#include "RHI/RHIShaderDefine.h"


namespace Engine
{
#define MAX_LIGHT_INSTANCES  80
#define MAX_SHADOW_INSTANCES 32

	static const int LightType_Directional = 0;
	static const int LightType_Point = 1;
	static const int LightType_Spot = 2;

	/** Deferred + shadow pass: punctual lights that own the single cubemap shadow use this ShadowMapIndex (not the directional map). */
	static constexpr int kPointLightCubeShadowMapIndex = 2;

	struct Light
	{
		math::Matrix4x4	LightViewProj;
		math::Matrix4x4 LightView;

		math::Vector3	Direction;
		float			Range{ 0.f };

		math::Vector3   Color;
		float			Intensity{ 1.f };

		math::Vector3   Position;
		float			InnerConeCos{ 0.f };

		float			OuterConeCos{ 0.f };
		int				Type{ 0 };
		float			DepthBias{ 0.f };
		int				ShadowMapIndex{ -1 };
	};

	struct MaterialPerFrame
	{
		float Metallic{ 0.f };
		float AlphaCutoff{ 0.5f };
		uint32_t AlphaMask{ 0 };
		uint32_t Padding{ 0 };
	};

	struct PerFrame
	{
		math::Matrix4x4     CameraCurrViewProj;
		math::Matrix4x4     CameraPrevViewProj;
		math::Matrix4x4     CameraCurrViewProjInverse;
		math::Matrix4x4     RotateIBL;
		math::Vector4       CameraPos;
		float				IBLFactor{ 0.f };
		float				EmissiveFactor{ 100.f };
		math::Vector2       InvScreenResolution;
		math::Vector4       WireframeOptions;
		float				LodBias{ 0.f };
		float				IBLMIpCount{ 1.f };
		int32_t				LightCount{ 0 };
		/** Packed 0/1; mirrors FSceneViewData::bUnlit (UE-style view unlit). */
		int32_t				bUnlit{ 0 };
		math::Vector4		TemporalAAJitter{ 1.f, 1.f, 1.f, 1.f };
		/** Matches deferred fullscreen reconstruct; must stay in sync with PerFrameStruct.hlsl. */
		float				CameraNearZ{ 0.1f };
		float				CameraFarZ{ 1000.f };
		uint32_t			PerFramePadBeforeLights[2]{};
		Light				Lights[MAX_LIGHT_INSTANCES];
		MaterialPerFrame	Material;
	};

	struct CBPerFrame
	{
		PerFrame myPerFrame;
	};
	static_assert(sizeof(CBPerFrame) % 16u == 0u, "D3D constant buffer size must be 16-byte aligned");
	static_assert(sizeof(CBPerFrame) <= 65536u, "D3D11/D3D12 cbuffer max size is 64KB");
	using CBPerFrameWrap = RenderCore::TUniformBufferBinding<CBPerFrame, 0u>;

	struct CBPointShadow
	{
		math::Matrix4x4 FaceVP[6];
		/** Matches DeferredLighting.hlsl cbPointShadow (reserved for shader parity / future use). */
		math::Vector4 LightPosRange{};
		int32_t Enabled = 0;
		int32_t LightIndex = -1;
		uint32_t Pad[2]{};
	};
	static_assert(sizeof(CBPointShadow) % 16u == 0u, "cbPointShadow must be 16-byte aligned");
	using CBPointShadowWrap = RenderCore::TUniformBufferBinding<CBPointShadow, 4u>;

	struct CBPerObject
	{
		math::Matrix4x4 myPerObject_u_mCurrWorld;
		math::Matrix4x4 myPerObject_u_mPrevWorld;
	};
	using CBPerObjectWrap = RenderCore::TUniformBufferBinding<CBPerObject, 1u>;

	struct Matrix2
	{
		math::Matrix4x4 Current;
		math::Matrix4x4 Previous;
	};

	struct CBPerSkeleton
	{
		// D3D11/D3D12 cbuffer limit 64KB; Matrix2 (curr+prev 4x4) = 128 bytes/bone → max 512 slots.
		static constexpr int kPaletteMatrixCount = 512;
		Matrix2 PerSkeleton_u_ModelMatrix[kPaletteMatrixCount];
	};
	static_assert(sizeof(CBPerSkeleton) % 16u == 0u, "D3D constant buffer size must be 16-byte aligned");
	static_assert(sizeof(CBPerSkeleton) <= 65536u, "D3D11/D3D12 cbuffer max size is 64KB");
	using CBPerSkeletonWrap = RenderCore::TUniformBufferBinding<CBPerSkeleton, 2u>;

	struct CBPerFur
	{
		math::Vector3 Gravity{};
		float FurOffset{};
		math::Vector3 FurColor{};
		float FurLength{};
		float UVScale{ 1.0f };
		float FurAmbientStrength{ 1.0f };
		float FurLevel{ 1.0f };
		float FurLightExposure{ 1.0f };
		uint32_t DrawSolid{ 0 };
		uint32_t FurPad1{};
		uint32_t FurPad2{};
		uint32_t FurPad3{};
	};
	using CBPerFurWrap = RenderCore::TUniformBufferBinding<CBPerFur, 3u>;

	struct ENVContant
	{
		float Exposure{};
		int32_t MipLevel{};
		int32_t MaxMipLevel{};
		int32_t NumSamplesPerDir{};
	};
	using ENVContantWrap = RenderCore::TUniformBufferBinding<ENVContant, 5u>;
}
