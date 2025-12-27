#pragma once
#include "core/inc.h"
#include "RHI/RHIShaderDefine.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
	class RHITexture2D;
}

namespace Engine
{
	struct FXAAPrivate;
	class FXAA
	{
	public:
		FXAA(RenderCore::DynamicRHI* RHI);
		~FXAA();

		void InitResource();
	private:
		FXAAPrivate* d_ptr;
	};
}


