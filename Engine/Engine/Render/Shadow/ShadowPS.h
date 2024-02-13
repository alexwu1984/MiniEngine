#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
	class RHITextureCube;
	class RHITexture2D;
}

namespace Engine
{
	struct ShadowPrivate;

	class ShadowPS
	{
	public:
		ShadowPS(RenderCore::DynamicRHI* rhi);
		~ShadowPS();

		void InitResource();
		void Draw(RenderCore::RHICommandContext& RHIContext);

	private:
		ShadowPrivate* d_ptr = nullptr;
	};
}