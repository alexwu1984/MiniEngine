#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
}

namespace Engine
{
	struct CubeRenderPrivate;

	class CubeRender
	{
	public:
		CubeRender(RenderCore::DynamicRHI* RHI);
		~CubeRender();
		void InitResource();
		void Render(RenderCore::RHICommandContext& RHIContext);
	private:
		CubeRenderPrivate* d_ptr = nullptr;
	};
	
	struct CubeMapCrossRenderPrivate;
	class CubeMapCrossRender
	{
	public:
		CubeMapCrossRender(RenderCore::DynamicRHI* RHI);
		~CubeMapCrossRender();
		void InitResource();
		void Render(RenderCore::RHICommandContext& RHIContext);
	private:
		CubeMapCrossRenderPrivate* d_ptr = nullptr;
	};
}