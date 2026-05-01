#pragma once
#include "core/inc.h"
#include "math/aabb3.h"
#include "math/matrix4x4.h"
#include "GltfModel/MeshBase.h"

struct aiScene;
struct aiMesh;
namespace Engine
{
	struct AssimpMeshPrivate;

	// Single mesh extracted via Assimp.
	class AssimpMesh : public MeshBase
	{
	public:
		// bFlipObjNormalZ: match DirectX12Tutorial FModel(obj, FlipV, NegateZ=false, FlipNormalZ=true) — flip vn.z for DX-style shading.
		AssimpMesh(const aiScene* pScene, aiMesh* pMesh, const std::string& Directory, bool bFlipObjNormalZ = false);
		~AssimpMesh();

		void Init();
		virtual const math::AABB3& GetBoundingBox() const override;
		virtual const math::Matrix4x4& GetMeshMat() const override;
		virtual std::shared_ptr<GltfMeshBuffer> GetMeshBuffer() override;
		virtual std::shared_ptr<MaterialBase> GetMaterial() override;
		virtual std::string GetMeshName() const override;
		virtual bool HasSkin() const override { return false; }
		virtual int32_t GetNodeId() const override { return -1; }
		virtual int32_t GetSkinId() const override { return -1; }
		virtual std::vector<std::vector<BoneSkinInfo>>& GetBoneNodeArray() override;
	private:
		void ProcessVertex();
		void ProcessIndices();
		void ProcessTextures();
	private:
		AssimpMeshPrivate* d_ptr = nullptr;
	};
}