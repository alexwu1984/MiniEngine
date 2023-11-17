#include "Render/GBuffer.h"
#include "RHI/RHITexture2D.h"
#include "RHI/DynamicRHI.h"
#include "Engine/Engine.h"

using namespace RenderCore;

namespace Engine
{
	struct GBufferPrivate
	{
		DynamicRHI* RHI = nullptr;
		std::shared_ptr<RHITexture2D> Depth;
		std::shared_ptr<RHITexture2D> SceneColor;
		std::shared_ptr<RHITexture2D> MotionVector;
		std::shared_ptr<RHITexture2D> NormalBuffer;
	};

	GBuffer::GBuffer(DynamicRHI* RHI)
		:d_ptr(new GBufferPrivate())
	{
		C_P(GBuffer);
		d->RHI = RHI;
	}

	GBuffer::~GBuffer()
	{
		delete d_ptr;
	}

	void GBuffer::InitResource(GBufferFlagBits Flag, uint32_t Width, uint32_t Height)
	{
		C_P(GBuffer);
		if (Flag & GBUFFER_DEPTH)
		{
			d->Depth = d->RHI->RHICreateTexture2D(EPixelFormat::PF_DepthStencil, ETextureCreateFlags::TexCreate_DepthStencilTargetable, Width, Height);
		}
		if (Flag & GBUFFER_MOTION_VECTORS)
		{
			d->MotionVector = d->RHI->RHICreateTexture2D(EPixelFormat::PF_G16R16, ETextureCreateFlags::TexCreate_RenderTargetable, Width, Height);
		}
		if (Flag & GBUFFER_SCENE_COLOR)
		{
			d->SceneColor = d->RHI->RHICreateTexture2D(EPixelFormat::PF_FloatRGBA, ETextureCreateFlags::TexCreate_RenderTargetable, Width, Height);
		}
		if (Flag & GBUFFER_NORMAL_BUFFER)
		{
			d->NormalBuffer = d->RHI->RHICreateTexture2D(EPixelFormat::PF_FloatRGBA, ETextureCreateFlags::TexCreate_RenderTargetable, Width, Height);
		}
	}

	std::shared_ptr<RHITexture2D> GBuffer::GetDepth() const
	{
		C_P(GBuffer);
		return d->Depth;
	}

	std::shared_ptr<RHITexture2D> GBuffer::GetSceneColor() const
	{
		C_P(GBuffer);
		return d->SceneColor;
	}

	std::shared_ptr<RenderCore::RHITexture2D> GBuffer::GetMotionVector() const
	{
		C_P(GBuffer);
		return d->MotionVector;
	}

	std::shared_ptr<RenderCore::RHITexture2D> GBuffer::GetNormalBuffer() const
	{
		C_P(GBuffer);
		return d->NormalBuffer;
	}

}