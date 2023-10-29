#include "Render/CubeRender.h"
#include "RHI/DynamicRHI.h"
#include "Engine/Engine.h"
#include "RHI/RHITextureCube.h"
#include "RHI/RHICommandContext.h"

namespace Engine
{
	struct CubeRenderPrivate
	{
		std::shared_ptr< RenderCore::RHIVertexBuffer> CubeVB;
		RenderCore::DynamicRHI* RHI;
	};

	CubeRender::CubeRender(RenderCore::DynamicRHI* RHI)
		:d_ptr(new CubeRenderPrivate())
	{
		C_P(CubeRender);
		d->RHI = RHI;
	}

	CubeRender::~CubeRender()
	{
		delete d_ptr;
	}

	void CubeRender::InitResource()
	{
		C_P(CubeRender);

		float vertices[] = {
			// back face
			-1.0f, -1.0f, -1.0f,
			1.0f, 1.0f, -1.0f,
			1.0f, -1.0f, -1.0f,
			1.0f, 1.0f, -1.0f,
			-1.0f, -1.0f, -1.0f,
			-1.0f, 1.0f, -1.0f,
			-1.0f, -1.0f, 1.0f,
			1.0f, -1.0f, 1.0f,
			1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f,
			-1.0f, 1.0f, 1.0f,
			-1.0f, -1.0f, 1.0f,
			-1.0f, 1.0f, 1.0f,
			-1.0f, 1.0f, -1.0f,
			-1.0f, -1.0f, -1.0f,
			-1.0f, -1.0f, -1.0f,
			-1.0f, -1.0f, 1.0f,
			-1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f,
			1.0f, -1.0f, -1.0f,
			1.0f, 1.0f, -1.0f,
			1.0f, -1.0f, -1.0f,
			1.0f, 1.0f, 1.0f,
			1.0f, -1.0f, 1.0f,
			-1.0f, -1.0f, -1.0f,
			1.0f, -1.0f, -1.0f,
			1.0f, -1.0f, 1.0f,
			1.0f, -1.0f, 1.0f,
			-1.0f, -1.0f, 1.0f,
			-1.0f, -1.0f, -1.0f,
			-1.0f, 1.0f, -1.0f,
			1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, -1.0f,
			1.0f, 1.0f, 1.0f,
			-1.0f, 1.0f, -1.0f,
			-1.0f, 1.0f, 1.0f,
		};
		d->CubeVB = d->RHI->RHICreateVertexBuffer(vertices, RenderCore::BUF_Dynamic, sizeof(math::Vector3), 36);
	}

	void CubeRender::Render(RenderCore::RHICommandContext& RHIContext)
	{
		C_P(CubeRender);
		RHIContext.DrawPrimitive(d->CubeVB);
	}
}