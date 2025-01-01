#include "GltfModel/DyTransfromNode.h"


using namespace math;

namespace Engine
{
	struct TransformNodePrivate
	{
		std::string Id;
		Vector3		LocalPosition;
		Vector3     WorldPosition;
		Quaternion	LocalRotation;
		Quaternion  WorldRotation;
		Matrix4x4	LocalToWorld;
		Matrix4x4	WorldToLocal;

		FDyTransformNode* FirstChild;
		FDyTransformNode* NextSibling;
		FDyTransformNode* PrevSibling;
		FDyTransformNode* Parent;
		int32_t ChildCount = 0;
	};

	FDyTransformNode::FDyTransformNode(const char* id)
		:Impl(new TransformNodePrivate())
	{
		Impl->Id = id;
	}

	FDyTransformNode::~FDyTransformNode()
	{
		delete Impl;
	}

	void FDyTransformNode::AddChildNode(FDyTransformNode* Child)
	{
		if (Child->Impl->Parent)
		{
			return;
		}

		if (Impl->FirstChild)
		{
			auto* n = Impl->FirstChild;
			while (n->Impl->NextSibling)
				n = n->Impl->NextSibling;
			n->Impl->NextSibling = Child;
			Child->Impl->PrevSibling = n;
		}
		else
		{
			Impl->FirstChild = Child;
		}
		Child->Impl->Parent = this;
		++Impl->ChildCount;
	}

	void FDyTransformNode::SetLocalPosition(const math::Vector3& Pos)
	{
		Impl->LocalPosition = Pos;
	}

	void FDyTransformNode::SetWorldPosition(const math::Vector3& Pos)
	{
		Impl->WorldPosition = Pos;
	}

	void FDyTransformNode::SetLocalRotation(const math::Quaternion& Rot)
	{
		Impl->LocalRotation = Rot;
	}

	void FDyTransformNode::SetWorldRotation(const math::Quaternion& Rot)
	{
		Impl->WorldRotation = Rot;
	}

	void FDyTransformNode::SetLocalToWorld(const math::Matrix4x4& Mat)
	{
		Impl->LocalToWorld = Mat;
	}

	void FDyTransformNode::SetWorldToLocal(const math::Matrix4x4& Mat)
	{
		Impl->WorldToLocal = Mat;
	}

	math::Vector3 FDyTransformNode::GetLocalPosition() const
	{
		return Impl->LocalPosition;
	}

	math::Vector3 FDyTransformNode::GetWorldPosition() const
	{
		return Impl->WorldPosition;
	}

	math::Quaternion FDyTransformNode::GetLocalRotation() const
	{
		return Impl->LocalRotation;
	}

	math::Quaternion FDyTransformNode::GetWorldRotation() const
	{
		return Impl->WorldRotation;
	}

	math::Matrix4x4 FDyTransformNode::GetWorldToLocal() const
	{
		return Impl->WorldToLocal;
	}

	math::Matrix4x4 FDyTransformNode::GetLocalToWorld() const
	{
		return Impl->LocalToWorld;
	}

	FDyTransformNode* FDyTransformNode::GetFirstChild() const
	{
		return Impl->FirstChild;
	}

	int32_t FDyTransformNode::GetChildCount() const
	{
		return Impl->ChildCount;
	}

	std::string FDyTransformNode::GetID() const
	{
		return Impl->Id;
	}

}