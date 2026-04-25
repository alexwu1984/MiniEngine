#include "Render/PostProcessFullscreenShaders.h"
#include "core/system.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHIShdader.h"

namespace Engine
{
	PostProcessFullscreenShaders::PostProcessFullscreenShaders(RenderCore::DynamicRHI* InRHI)
		: RHI(InRHI)
	{
	}

	void PostProcessFullscreenShaders::InitResource()
	{
		std::wstring ShaderPath = core::process_directory().wstring() + L"/ShaderLibDX/PostProcess.hlsl";

		VertexShader = RHI->RHICreateVertexShader(ShaderPath, "VS_ScreenQuad", {}, {});
	}

	std::shared_ptr<RenderCore::RHIVertexShader> PostProcessFullscreenShaders::GetVertexShader() const
	{
		return VertexShader;
	}
}
