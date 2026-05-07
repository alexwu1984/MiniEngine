#include "GltfModel/GltfNode.h"
#include "math/quaternion.h"

namespace Engine
{
	using namespace math;

	namespace
	{
		/**
		 * glTF node.matrix is column-major for column-vector math v' = M * v.
		 * Engine + HLSL use row-vector mul(pos, W) i.e. pos' = pos * W, so W = M^T for the same mapping.
		 * (Unpack column-major to a mathematical M, then transpose for row-vector storage.)
		 */
		Matrix4x4 MatrixFromGltfNodeForRowVectorMultiply(const std::vector<double>& col)
		{
			if (col.size() != 16)
				return Matrix4x4();
			const Matrix4x4 M(
				(float)col[0], (float)col[4], (float)col[8], (float)col[12],
				(float)col[1], (float)col[5], (float)col[9], (float)col[13],
				(float)col[2], (float)col[6], (float)col[10], (float)col[14],
				(float)col[3], (float)col[7], (float)col[11], (float)col[15]);
			return M.Transpose();
		}
	}

	GltfNode::GltfNode(tinygltf::Model* Model)
		:_Model(Model)
	{
		InitNode();
	}

	GltfNode::~GltfNode()
	{

	}

	void GltfNode::InitGroupNode(uint32_t nodeIndex)
	{
		_GroupNode.reserve(_Model->nodes.size());
		if (nodeIndex >= 0 && nodeIndex < _Model->nodes.size())
		{
			std::shared_ptr<GltfNodeInfo> GroupNode = _Node[nodeIndex];

			auto& child = _Model->nodes[nodeIndex].children;
			if (child.size() > 0)
				CreateModelNodeTree(GroupNode);

			// Always DFS each scene root (including leaf roots with a mesh). Skipping leaf roots left their
			// subtrees outside _GroupNode so world transforms never matched other exporters' layouts.
			Matrix4x4 Identity;
			DFSNodeTree(GroupNode, Identity);
			_GroupNode.push_back(GroupNode);
		}
	}

	void GltfNode::UpdateNode()
	{
		for (int i = 0; i < _GroupNode.size(); i++)
		{
			Matrix4x4 Identity;
			DFSNodeTree(_GroupNode[i], Identity);
		}
	}

	void GltfNode::UpdateNodeParent(std::shared_ptr<GltfNodeInfo> NodeInfo)
	{
		// glTF local transform is M_col = T·R·S (column vectors). Engine uses row vectors p' = p·W with W = M_col^T = S^T·R^T·T^T.
		// With our row-major translation/rotation helpers that correspond to those transposed factors, compose local TRS as S·R·T (must match InitNode).
		Matrix4x4 NodeTransformation = NodeInfo->BaseMatrixFromGltf;
		NodeTransformation *= Matrix4x4::ScaleMatrix(NodeInfo->Scale);
		NodeTransformation *= Matrix4x4::CreateFromQuaternion(Quaternion(NodeInfo->Rotation));
		NodeTransformation *= Matrix4x4::CreateFromTranslate(NodeInfo->Translate);
		auto ParentNode = NodeInfo->ParentNode.lock();
		if (ParentNode)
		{
			UpdateNodeParent(ParentNode);
			// math::Vector4 * Matrix4x4 is row-vector p' = p * M (see vector4.cpp). Hierarchy: p_world = p * L * P.
			// DFSNodeTree does the same with FinalMeshMat = local * ParentMatrix — do not use parent * local here.
			NodeInfo->FinalMeshMat = NodeTransformation * ParentNode->FinalMeshMat;
		}
		else
		{
			NodeInfo->FinalMeshMat = NodeTransformation;
		}
	}


	std::shared_ptr<Engine::GltfNodeInfo> GltfNode::GetNodeInfo(int32_t NodeId)
	{
		if (NodeId < _Node.size())
		{
			return _Node[NodeId];
		}
		return nullptr;
	}

	void GltfNode::InitNode()
	{
		const auto& Nodes = _Model->nodes;
		_Node.reserve(Nodes.size());
		for (int i = 0; i < Nodes.size(); ++i)
		{
			const auto& Node = Nodes[i];

			std::shared_ptr< GltfNodeInfo> NodeInfo = std::make_shared<GltfNodeInfo>();
			NodeInfo->MeshID = Node.mesh;
			NodeInfo->SkinID = Node.skin;
			NodeInfo->NodeID = i;

			if (Node.matrix.size() == 16)
			{
				// glTF 2.0: matrix and TRS must not be combined — Unity exporters often emit both; ignore TRS here.
				const Matrix4x4 NodeMat = MatrixFromGltfNodeForRowVectorMultiply(Node.matrix);
				NodeInfo->BaseMatrixFromGltf = NodeMat;
				NodeInfo->Translate = Vector3::Zero;
				NodeInfo->Scale = Vector3(1.f, 1.f, 1.f);
				NodeInfo->Rotation = Quaternion::Identity;
				NodeInfo->FinalMeshMat = NodeMat;
			}
			else
			{
				NodeInfo->BaseMatrixFromGltf.Identity();
				if (Node.translation.size() == 3)
					NodeInfo->Translate = Vector3(float(Node.translation[0]), float(Node.translation[1]), float(Node.translation[2]));
				if (Node.scale.size() == 3)
					NodeInfo->Scale = Vector3(float(Node.scale[0]), float(Node.scale[1]), float(Node.scale[2]));
				if (Node.rotation.size() == 4)
					NodeInfo->Rotation = Quaternion(float(Node.rotation[0]), float(Node.rotation[1]), float(Node.rotation[2]), float(Node.rotation[3]));

				Matrix4x4 NodeMat;
				NodeMat.Identity();
				NodeMat *= Matrix4x4::ScaleMatrix(NodeInfo->Scale);
				NodeMat *= Matrix4x4::CreateFromQuaternion(Quaternion(NodeInfo->Rotation));
				NodeMat *= Matrix4x4::CreateFromTranslate(NodeInfo->Translate);

				NodeInfo->FinalMeshMat = NodeMat;
			}

			_Node.push_back(NodeInfo);
		}
	}

	void GltfNode::CreateModelNodeTree(std::shared_ptr<GltfNodeInfo> NodeInfo)
	{
		int NodeID = NodeInfo->NodeID;
		auto& child = _Model->nodes[NodeID].children;
		for (int i = 0; i < child.size(); i++)
		{
			int nodeId = child[i];
			std::shared_ptr<GltfNodeInfo> pChild = _Node[nodeId];
			pChild->ParentNode = NodeInfo;
			NodeInfo->ChildrenNode.push_back(pChild);

			CreateModelNodeTree(pChild);
		}
	}

	void GltfNode::DFSNodeTree(std::shared_ptr<GltfNodeInfo> NodeInfo, math::Matrix4x4& ParentMatrix)
	{
		Matrix4x4 NodeTransformation = NodeInfo->FinalMeshMat * ParentMatrix;

		NodeInfo->FinalMeshMat = NodeTransformation;

		for (int i = 0; i < NodeInfo->ChildrenNode.size(); i++)
		{
			DFSNodeTree(NodeInfo->ChildrenNode[i], NodeTransformation);
		}
	}

}