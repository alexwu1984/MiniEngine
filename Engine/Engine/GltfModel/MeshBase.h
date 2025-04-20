#pragma once
#include "core/inc.h"
#include "math/aabb3.h"

namespace Engine
{
	class GltfMeshBuffer;
	class MaterialBase;
	struct BoneSkinInfo;
	struct GltfBoneNodeInfo;

	class MeshBase
	{
	public:
		MeshBase() {};
		virtual ~MeshBase() {};
		virtual std::shared_ptr<GltfMeshBuffer> GetMeshBuffer() = 0;
		virtual std::shared_ptr<MaterialBase> GetMaterial() = 0;
		virtual const math::AABB3& GetBoundingBox() const = 0;
		virtual const math::Matrix4x4& GetMeshMat() const = 0;
		virtual std::string GetMeshName() const = 0;
		virtual bool HasSkin() const = 0;
		virtual int32_t GetNodeId() const = 0;
		virtual int32_t GetSkinId() const = 0;
		virtual std::vector<std::vector<BoneSkinInfo>>& GetBoneNodeArray() = 0;
	};
}