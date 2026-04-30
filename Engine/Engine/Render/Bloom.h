#pragma once
#include "core/inc.h"
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
	class GBuffer;

	BEGIN_SHADER_STRUCT(BloomContants, 0)
		DECLARE_PARAM_VALUE(float, BloomIntensity, 0.5f)
		DECLARE_PARAM_VALUE(float, BloomThreshold, 1.0f)
		/** exp2(exposure stops): applied to linear scene before tonemap; scales bloom extract threshold (see PostProcess.hlsl). */
		DECLARE_PARAM_VALUE(float, PostExposureLinear, 1.0f)
		DECLARE_PARAM_VALUE(float, Pad0, 0.0f)
		BEGIN_STRUCT_CONSTRUCT(BloomContants)
		END_STRUCT_CONSTRUCT
	END_SHADER_STRUCT

	class Bloom
	{
	public:
		Bloom(RenderCore::DynamicRHI* RHI);
		~Bloom();

		void InitResource();
		// Drop UAV mips (e.g. after GBuffer resize). Shaders stay valid.
		void InvalidateTransientResources();
		void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer);
		std::shared_ptr<RenderCore::RHITexture2D> GetResult() const;
	private:
		BloomPrivate* d_ptr = nullptr;
	};
}
