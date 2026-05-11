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

	/**
	 * Shadow slot markers on `Light::ShadowMapIndex` (actual textures are bound separately in deferred).
	 * Policy: one directional shadow map → **first** directional in the view list (`PrimaryDirectionalLightIndex`);
	 * one cubemap + one spot depth → **first** point / spot that opts into shadow in traverse order (see ShadowRenderPass).
	 * All lights still participate in analytic shading up to `MAX_LIGHT_INSTANCES`.
	 */
	static constexpr int kPointLightCubeShadowMapIndex = 2;
	static constexpr int kSpotLightShadowMapIndex = 3;

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
		/** Bit flags — keep sizeof(MaterialPerFrame)==16 and match PerFrameStruct.hlsl / ShadowPass-PS cbPerMaterial tail uint. */
		uint32_t MaterialShaderFlags{ 0 };
	};
	inline constexpr uint32_t kMaterialShaderFlag_WriteBaseColorAlphaToGBuffer = 1u << 0;
	inline constexpr uint32_t kMaterialShaderFlag_DoubleSidedShading = 1u << 1;
	inline constexpr uint32_t kMaterialShaderFlag_ShadowAlphaClip = 1u << 2;

	struct PerFrame
	{
		// View / camera matrices
		math::Matrix4x4     CameraCurrViewProj;
		math::Matrix4x4     CameraPrevViewProj;
		math::Matrix4x4     CameraCurrViewProjInverse;
		math::Matrix4x4     RotateIBL;

		// View / camera parameters
		math::Vector4       CameraPos;
		math::Vector2       InvScreenResolution;
		float				CameraNearZ{ 0.1f };
		float				CameraFarZ{ 1000.f };

		// IBL / material globals
		float				IBLFactor{ 0.f };
		float				EmissiveFactor{ 100.f };
		float				LodBias{ 0.f };
		float				IBLMIpCount{ 1.f };

		/** Procedural sky: blend sky irradiance/spec cubemap (upper) vs GroundIBLHdr lat-long (lower) by world-Y hemisphere. */
		float				GroundIBLIntensity{ 1.f };
		float				HemiIBLBlendPower{ 1.75f };
		int32_t				SplitHemisphereIBL{ 0 };
		int32_t				_PadSplitHemiIBL{ 0 };

		// Debug / view flags
		math::Vector4       WireframeOptions;
		int32_t				LightCount{ 0 };
		/**
		 * Index into Lights[] for the single directional shadow map (t8) / GetMainLightViewProj in base pass.
		 * Always the first directional in GatherLightsForView order (highest SortPriority among directionals). -1 if no directional.
		 */
		int32_t				PrimaryDirectionalLightIndex{ -1 };
		/** Packed 0/1; mirrors FSceneViewData::bUnlit (UE-style view unlit). */
		int32_t				bUnlit{ 0 };
		int32_t				_PadPerFrameLightHeader{ 0 };
		math::Vector4		TemporalAAJitter{ 1.f, 1.f, 1.f, 1.f };

		Light				Lights[MAX_LIGHT_INSTANCES];

		/** Trailing 16B: keeps sizeof(CBPerFrame) % 16 == 0. */
		uint32_t			PerFramePadAfterLights[4]{};
	};

	struct CBPerFrame
	{
		PerFrame myPerFrame;
	};
	static_assert(sizeof(CBPerFrame) % 16u == 0u, "D3D constant buffer size must be 16-byte aligned");
	static_assert(sizeof(CBPerFrame) <= 65536u, "D3D11/D3D12 cbuffer max size is 64KB");
	using CBPerFrameWrap = RenderCore::TUniformBufferBinding<CBPerFrame, 0u>;

	struct CBPerMaterial
	{
		MaterialPerFrame myMaterial{};
	};
	static_assert(sizeof(CBPerMaterial) % 16u == 0u, "cbPerMaterial must be 16-byte aligned");
	using CBPerMaterialWrap = RenderCore::TUniformBufferBinding<CBPerMaterial, 6u>;

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

	/** Matches DeferredLighting.hlsl / FurMaterial.hlsl cbSpotShadow (register b5). */
	struct CBSpotShadow
	{
		math::Matrix4x4 SpotLightViewProj{};
		int32_t SpotShadowEnabled = 0;
		int32_t SpotShadowLightIndex = -1;
		uint32_t _Pad0[2]{};
		uint32_t _Pad1[4]{};
	};
	static_assert(sizeof(CBSpotShadow) % 16u == 0u, "cbSpotShadow must be 16-byte aligned");
	using CBSpotShadowWrap = RenderCore::TUniformBufferBinding<CBSpotShadow, 5u>;

	/** Matches DeferredLighting.hlsl cbDirectionalShadowCSM (register b7). */
	struct CBDirectionalShadowCSM
	{
		math::Matrix4x4 CascadeViewProj[3]{};
		math::Vector4 CascadeSplits{};
		math::Vector4 CameraForwardInvCount{};
		int32_t DirectionalCSMEnabled = 0;
		int32_t _PadDirectionalCSM[3]{};
	};
	static_assert(sizeof(CBDirectionalShadowCSM) % 16u == 0u, "cbDirectionalShadowCSM must be 16-byte aligned");
	using CBDirectionalShadowCSMWrap = RenderCore::TUniformBufferBinding<CBDirectionalShadowCSM, 7u>;

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
