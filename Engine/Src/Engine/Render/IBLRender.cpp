#include "Render/IBLRender.h"
#include "RHI/DynamicRHI.h"
#include "Engine/Engine.h"
#include "RHI/RHITextureCube.h"

namespace Engine
{
	struct IBLRenderPrivate
	{
		std::shared_ptr<RenderCore::RHITextureCube> PreFilterCube;
		std::shared_ptr<RenderCore::RHITextureCube> IrrCube;
	};

	IBLRender::IBLRender()
		:d_ptr(new IBLRenderPrivate())
	{

	}

	IBLRender::~IBLRender()
	{

	}

	void IBLRender::InitResource(RenderCore::DynamicRHI* RHI)
	{
		RHI->RHICreateTextureCube(RenderCore::PF_A16B16G16R16, 32, 32, 5, false);
		
	}

	void IBLRender::Draw(RenderCore::RHICommandContext& RHIContext)
	{

	}

}
