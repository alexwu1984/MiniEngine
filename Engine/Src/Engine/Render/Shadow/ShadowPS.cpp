#include "Render/Shadow/ShadowPS.h"
#include "RHI/DynamicRHI.h"
#include "Engine/Engine.h"
#include "core/system.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIShaderDefine.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "Render/MaterialPreFrame.h"
#include "Render/Blur.h"

namespace Engine
{
	struct ShadowPrivate
	{
		ShadowPrivate(RenderCore::DynamicRHI* _rhi)
			:rhi(_rhi)
		{

		}
		RenderCore::DynamicRHI* rhi;
		std::shared_ptr< RenderCore::RHIVertexShader> vs;
		std::shared_ptr< RenderCore::RHIPixelShader> ps;
		std::shared_ptr< BlurCS> blur;
	};

	ShadowPS::ShadowPS(RenderCore::DynamicRHI* rhi)
		:d_ptr(new ShadowPrivate(rhi))
	{

	}

	ShadowPS::~ShadowPS()
	{
		delete d_ptr;
	}

	void ShadowPS::InitResource()
	{

	}

	void ShadowPS::Draw(RenderCore::RHICommandContext& RHIContext)
	{

	}

}