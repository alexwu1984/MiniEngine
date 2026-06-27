#pragma once
#include "Render/MaterialPreFrame.h"

namespace RenderCore
{
	class DynamicRHI;
	class RHICommandContext;
	class RHIRenderTarget;
	class RHITextureCube;
}

namespace Engine
{
	struct GltfSceneMeshInfo;
	class MeshBase;
	class FShadowPassMeshDraw;

	/** UE-style: shared depth draws for directional / spot / point cube faces; owns FShadowPassMeshDraw cache per mesh. */
	class FShadowDepthMeshDrawer
	{
	public:
		explicit FShadowDepthMeshDrawer(RenderCore::DynamicRHI* InRHI);

		void PruneStaleMeshShadowPasses(const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes, const std::vector<GltfSceneMeshInfo>& FrustumBoundsMeshes);
		void ClearCache();

		void DrawDirectional(RenderCore::RHICommandContext& RHIContext, const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes, const Light& LightForShadow,
							 const std::shared_ptr<RenderCore::RHIRenderTarget>& Target);

		void DrawCubeFace(RenderCore::RHICommandContext& RHIContext, const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes, const Light& FaceLight,
						  const std::shared_ptr<RenderCore::RHITextureCube>& Cube, int FaceIndex);

	private:
		static void UpdatePassMeshDrawPalette(const std::shared_ptr<FShadowPassMeshDraw>& PassMeshDraw, const std::shared_ptr<MeshBase>& Mesh);

		RenderCore::DynamicRHI* RHI = nullptr;
		std::map<std::shared_ptr<MeshBase>, std::shared_ptr<FShadowPassMeshDraw>> PassMeshDraws;
	};
}
