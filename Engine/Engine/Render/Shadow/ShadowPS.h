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
	class RHIRenderTarget;
}

namespace Engine
{
	struct ShadowPSPrivate;
	class MeshBase;

	class ShadowPS
	{
	public:
		ShadowPS(RenderCore::DynamicRHI* RHI, std::shared_ptr<MeshBase> gltfMesh);
		~ShadowPS();

		void InitResource();
		void Draw(RenderCore::RHICommandContext& RHIContext, const math::Matrix4x4& WorldTransform,
			const Light& mainLight, std::shared_ptr<RenderCore::RHIRenderTarget> renderTarget);
		void DrawCubeFace(RenderCore::RHICommandContext& RHIContext, const math::Matrix4x4& WorldTransform,
			const Light& faceLight, std::shared_ptr<RenderCore::RHITextureCube> cube, int32_t faceIndex);
		void SetBoneMatrix(const math::Matrix4x4& Mat, int32_t Index);
		void ResetSkeletonPaletteIdentity();
	private:
		ShadowPSPrivate* d_ptr = nullptr;
	};
}