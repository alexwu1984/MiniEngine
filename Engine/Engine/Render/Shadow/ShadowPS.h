#pragma once
#include "core/inc.h"
#include "math/matrix4x4.h"
#include "Render/MaterialPreFrame.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
	class RHITextureCube;
	class RHITexture2D;
}

namespace Engine
{
	struct ShadowPSPrivate;
	class GltfMesh;

	class ShadowPS
	{
	public:
		ShadowPS(RenderCore::DynamicRHI* RHI, std::shared_ptr<GltfMesh> gltfMesh);
		~ShadowPS();

		void InitResource();
		void Draw(RenderCore::RHICommandContext& RHIContext, const math::Matrix4x4& WorldTransform,const Light& mainLight);

	private:
		ShadowPSPrivate* d_ptr = nullptr;
	};
}