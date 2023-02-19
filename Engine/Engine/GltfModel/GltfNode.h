#pragma once
#include "math/matrix4x4.h"
#include "tinygltf/tiny_gltf.h"

namespace Engine
{
	/****************************************************
*	Node结构体
*	每个Node需要包含：
*		Mesh 变换矩阵
****************************************************/
	struct GltfNodeInfo
	{
		GltfNodeInfo()
		{
			Rotation = math::Vector4(0, 0, 0, 1);
			Scale = math::Vector3(1, 1, 1);
			Translate = math::Vector3(0, 0, 0);

			MeshID = -1;
			SkinID = -1;
			NodeID = -1;
			ParentNode;
		}
		~GltfNodeInfo()
		{
			ChildrenNode.clear();
		}

		math::Vector4 Rotation;
		math::Vector3 Scale;
		math::Vector3 Translate;

		int MeshID;
		int SkinID;

		int NodeID;
		std::weak_ptr<GltfNodeInfo> ParentNode;
		std::vector<std::shared_ptr<GltfNodeInfo>> ChildrenNode;

		math::Matrix4x4 FinalMeshMat;
	};

	class GltfNode
	{
	public:
		GltfNode(tinygltf::Model* Model);
		~GltfNode();

		void InitGroupNode(uint32_t nodeIndex);
		void UpdateNode();
		void UpdateNodeParent(std::shared_ptr<GltfNodeInfo> NodeInfo);
		const std::vector<std::shared_ptr<GltfNodeInfo>>& GetAllNodes() const
		{
			return _Node;
		}
		std::shared_ptr<GltfNodeInfo> GetNodeInfo(int32_t NodeId);
	private:
		void InitNode();
		void CreateModelNodeTree(std::shared_ptr<GltfNodeInfo> NodeInfo);
		void DFSNodeTree(std::shared_ptr<GltfNodeInfo> NodeInfo, math::Matrix4x4& ParentMatrix);
	private:
		tinygltf::Model* _Model;

		std::vector<std::shared_ptr<GltfNodeInfo>> _Node;
		std::vector<std::shared_ptr<GltfNodeInfo>> _GroupNode;
	};
}