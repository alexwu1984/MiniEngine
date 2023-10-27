#include "Render/GBuffer.h"
#include "RHI/RHITexture2D.h"
#include "RHI/DynamicRHI.h"
#include "Engine/Engine.h"

namespace Engine
{
	struct GBufferPrivate
	{
		std::shared_ptr< RenderCore::RHITexture2D> Depth;
		std::shared_ptr< RenderCore::RHITexture2D> SceneColor;
		std::shared_ptr< RenderCore::RHITexture2D> MotionVector;
	};

	GBuffer::GBuffer()
		:d_ptr(new GBufferPrivate())
	{

	}

	GBuffer::~GBuffer()
	{
		delete d_ptr;
	}

	void GBuffer::InitBuffer(GBufferFlagBits Flag)
	{

	}

}