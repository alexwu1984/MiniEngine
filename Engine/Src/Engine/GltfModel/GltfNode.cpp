#include "GltfModel/GltfNode.h"
#include "math/quaternion.h"

namespace Engine
{
	using namespace math;

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
			{
				CreateModelNodeTree(GroupNode);

				Matrix4x4 Identity;

				DFSNodeTree(GroupNode, Identity);

				_GroupNode.push_back(GroupNode);
			}
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
		Matrix4x4 mat4Scaling = Matrix4x4::ScaleMatrix(NodeInfo->Scale);
		Matrix4x4 mat4Rotation = Matrix4x4::CreateFromQuaternion(Quaternion(NodeInfo->Rotation));
		Matrix4x4 mat4Translation = Matrix4x4::CreateFromTranslate(NodeInfo->Translate);

		Matrix4x4 NodeTransformation = mat4Translation * mat4Rotation * mat4Scaling;
		auto ParentNode = NodeInfo->ParentNode.lock();
		if (ParentNode)
		{
			UpdateNodeParent(ParentNode);
			NodeInfo->FinalMeshMat = ParentNode->FinalMeshMat * NodeTransformation;
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

			Matrix4x4 NodeMat;
			if (Node.matrix.size() == 16)
			{
				NodeMat = Matrix4x4(float(Node.matrix[0]), float(Node.matrix[1]), float(Node.matrix[2]), float(Node.matrix[3]),
					float(Node.matrix[4]), float(Node.matrix[5]), float(Node.matrix[6]), float(Node.matrix[7]),
					float(Node.matrix[8]), float(Node.matrix[9]), float(Node.matrix[10]), float(Node.matrix[11]),
					float(Node.matrix[12]), float(Node.matrix[13]), float(Node.matrix[14]), float(Node.matrix[15]));
			}

			if (Node.translation.size() == 3)
			{
				NodeInfo->Translate = Vector3(float(Node.translation[0]), float(Node.translation[1]), float(Node.translation[2]));
				NodeMat *= Matrix4x4::CreateFromTranslate(NodeInfo->Translate);
				
			}

			if (Node.scale.size() == 3)
			{
				NodeInfo->Scale = Vector3(float(Node.scale[0]), float(Node.scale[1]), float(Node.scale[2]));
				NodeMat *= Matrix4x4::ScaleMatrix(NodeInfo->Scale);
				
			}

			if (Node.rotation.size() == 4)
			{
				NodeInfo->Rotation = Quaternion(float(Node.rotation[0]), float(Node.rotation[1]), float(Node.rotation[2]), float(Node.rotation[3]));
				NodeMat *= Matrix4x4::CreateFromQuaternion(Quaternion(NodeInfo->Rotation));
				
			}

			NodeInfo->FinalMeshMat = NodeMat;

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
		Matrix4x4 NodeTransformation = ParentMatrix * NodeInfo->FinalMeshMat;

		NodeInfo->FinalMeshMat = NodeTransformation;

		for (int i = 0; i < NodeInfo->ChildrenNode.size(); i++)
		{
			DFSNodeTree(NodeInfo->ChildrenNode[i], NodeTransformation);
		}
	}

}