#pragma once
#include "core/inc.h"
#include "RHI/RHIShaderDefine.h"

namespace RenderCore
{
	class SceneTextures;
	class RHICommandContext;
	class DynamicRHI;
	class RHITexture2D;
	struct FXAAPrivate;

	class FXAA
	{
	public:
		FXAA(DynamicRHI* RHI);
		~FXAA();
		void InitResource();
		void InvalidateTransientResources();
		void Draw(RHICommandContext& RHIContext, std::shared_ptr<RHITexture2D> TargetBuffer);
		std::shared_ptr<RHITexture2D> GetResult() const;
	private:
		FXAAPrivate* d_ptr;
	};
}


