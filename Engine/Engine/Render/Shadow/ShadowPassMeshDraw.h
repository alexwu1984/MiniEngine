#pragma once
#include "math/matrix4x4.h"
#include "Render/MaterialPreFrame.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
	class RHITextureCube;
	class RHIRenderTarget;
}

namespace Engine
{
	struct FShadowPassMeshDrawPrivate;
	class MeshBase;

	/** Per-mesh shadow depth draw (VS/PS + CBs) for directional, spot, and point cube faces. */
	class FShadowPassMeshDraw
	{
	public:
		FShadowPassMeshDraw(RenderCore::DynamicRHI* RHI, std::shared_ptr<MeshBase> Mesh);
		~FShadowPassMeshDraw();

		void InitResource();
		void Draw(RenderCore::RHICommandContext& RHIContext, const math::Matrix4x4& WorldTransform, const Light& mainLight,
				  std::shared_ptr<RenderCore::RHIRenderTarget> renderTarget);
		void DrawCubeFace(RenderCore::RHICommandContext& RHIContext, const math::Matrix4x4& WorldTransform, const Light& faceLight,
						  std::shared_ptr<RenderCore::RHITextureCube> cube, int32_t faceIndex);
		void SetBoneMatrix(const math::Matrix4x4& Mat, int32_t Index);
		void ResetSkeletonPaletteIdentity();

	private:
		FShadowPassMeshDrawPrivate* d_ptr = nullptr;
	};
}
