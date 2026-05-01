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

	struct CubeMapCrossRenderPrivate
	{
		std::array<std::shared_ptr<RenderCore::RHIVertexBuffer>, RenderCore::EVertexType::VT_Max> VerticesBuffer;
		std::shared_ptr<RenderCore::RHIIndexBuffer> IndexBuffer;
		RenderCore::DynamicRHI* RHI = nullptr;
	};

	CubeMapCrossRender::CubeMapCrossRender(RenderCore::DynamicRHI* RHI)
		:d_ptr(new CubeMapCrossRenderPrivate())
	{
		C_P(CubeMapCrossRender);
		d->RHI = RHI;
	}

	CubeMapCrossRender::~CubeMapCrossRender()
	{
		delete d_ptr;
	}

	void CubeMapCrossRender::InitResource()
	{
		C_P(CubeMapCrossRender);
		std::vector<math::Vector3> positions{
			{ -0.25f, 0.375f, 0.0f, },
			{ 0.0f, 0.375f, 0.0f, },
			{ -0.5f, 0.125f, 0.0f, },
			{ -0.25f, 0.125f, 0.0f, },
			{ 0.0f, 0.125f, 0.0f, },
			{ 0.25f, 0.125f, 0.0f, },
			{ 0.5f, 0.125f, 0.0f, },
			{ -0.5f, -0.125f, 0.0f, },
			{ -0.25f, -0.125f, 0.0f, },
			{ 0.0f, -0.125f, 0.0f, },
			{ 0.25f, -0.125f, 0.0f, },
			{ 0.5f, -0.125f, 0.0f, },
			{ -0.25f, -0.375f, 0.0f, },
			{ 0.0f, -0.375f, 0.0f, },
		};
		d->VerticesBuffer[RenderCore::VT_Position] = d->RHI->RHICreateVertexBuffer(&positions[0].x, RenderCore::BUF_VertexBuffer, sizeof(math::Vector3), positions.size());

		std::vector<math::Vector3> normals = {
				{-1,  1, -1 },
				{1, 1, -1 },
				{-1, 1, -1 },
				{-1,  1,  1 },
				{1,  1,  1 },
				{1,  1, -1 },
				{-1,  1, -1 },
				{-1, -1, -1 },
				{-1, -1,  1 },
				{1, -1,  1 },
				{1, -1, -1 },
				{-1, -1, -1 },
				{-1, -1, -1 },
				{1, -1, -1 }
		};
		d->VerticesBuffer[RenderCore::VT_Normal] = d->RHI->RHICreateVertexBuffer(&normals[0].x, RenderCore::BUF_VertexBuffer, sizeof(math::Vector3), normals.size());

		std::vector<uint32_t>  indices = {
		0, 1, 3, 3, 1, 4,
		2, 3, 7, 7, 3, 8,
		3, 4, 8, 8, 4, 9,
		4, 5, 9, 9, 5, 10,
		5, 6, 10, 10, 6, 11,
		8, 9, 12, 12, 9, 13
		};
		d->IndexBuffer = d->RHI->RHICreateIndexBuffer(indices.data(), RenderCore::BUF_IndexBuffer, indices.size()/3);
	}

	void CubeMapCrossRender::Render(RenderCore::RHICommandContext& RHIContext)
	{
		C_P(CubeMapCrossRender);
		RHIContext.DrawPrimitive(d->VerticesBuffer, d->IndexBuffer);
	}

}