#pragma once
#include "RHI/RHIShaderDefine.h"
#include "math/vector2.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
	class RHITexture2D;
}

namespace Engine
{
	struct BloomPrivate;
	class FSceneTextures;

	struct BloomContants
	{
		float BloomIntensity{ 0.5f };
		/** Linear luminance threshold before bloom extract (scaled by PostExposureLinear). Lower = easier sun/sky bloom. */
		float BloomThreshold{ 0.72f };
		/** exp2(exposure stops): applied to linear scene before tonemap; scales bloom extract threshold (see PostProcess.hlsl). */
		float PostExposureLinear{ 1.0f };
		float Pad0{ 0.0f };
	};
	using BloomContantsWrap = RenderCore::TUniformBufferBinding<BloomContants, 0u>;

	class Bloom
	{
	public:
		Bloom(RenderCore::DynamicRHI* RHI);
		~Bloom();

		void InitResource();
		// Drop UAV mips (e.g. after scene texture resize). Shaders stay valid.
		void InvalidateTransientResources();
		void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<FSceneTextures> SceneTextures);
		std::shared_ptr<RenderCore::RHITexture2D> GetResult() const;
	private:
		BloomPrivate* d_ptr = nullptr;
	};
}
