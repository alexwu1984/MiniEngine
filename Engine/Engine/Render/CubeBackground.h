#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
	class RHITextureCube;
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
		void Render(RenderCore::RHICommandContext& RHIContext);
		void SetTextureCube(std::shared_ptr<RenderCore::RHITextureCube> TexCube);
		void SetRotate(float xRotate, float yRotate);
	private:
		void InitShader();
	private:
		CubeBackgroundPrivate* d_ptr = nullptr;
	};
}