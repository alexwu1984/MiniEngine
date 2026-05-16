#pragma once
#include "RHI/RHIShaderDefine.h"

namespace RenderCore
{
	class DynamicRHI;
	class RHICommandContext;
	class RHITexture2D;
}

namespace Engine
{
	struct FXAAPrivate;

	/** Post-process AA lives next to RDG helpers (FRDGUtils); avoids RenderCore depending on Engine barrier helpers. */
	class FXAA
	{
	public:
		FXAA(RenderCore::DynamicRHI* RHI);
		~FXAA();
		void InitResource();
		void InvalidateTransientResources();
		void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHITexture2D> SourceTexture);
		std::shared_ptr<RenderCore::RHITexture2D> GetResult() const;

	private:
		FXAAPrivate* d_ptr;
	};
}
