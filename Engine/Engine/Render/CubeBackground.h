#pragma once
#include "core/inc.h"
#include "math/matrix4x4.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
	class RHITextureCube;
	class RHITexture2D;
}


namespace Engine
{
	struct CubeBackgroundPrivate;

	class CubeBackground
	{
	public:
		CubeBackground(RenderCore::DynamicRHI* RHI);
		~CubeBackground();

		void InitResource();
		void Render(RenderCore::RHICommandContext& RHIContext,
					const std::vector<std::shared_ptr<RenderCore::RHITexture2D>>& Targets,
					std::shared_ptr<RenderCore::RHITexture2D> Depth,
					const math::Matrix4x4& ViewMatrix,
					const math::Matrix4x4& ProjMatrix);
		void SetTextureCube(std::shared_ptr<RenderCore::RHITextureCube> TexCube);
	private:
		void InitShader();
	private:
		CubeBackgroundPrivate* d_ptr = nullptr;
	};
}