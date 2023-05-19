#include "GltfModel/DyTransfromNode.h"
#include "math/vector3.h"
#include "math/quaternion.h"

using namespace math;

namespace Engine
{
	struct TransformNodeP
	{
		std::string Id;
		Vector3		LocalPosition;
		Quaternion	LocalRotation;

		DyTransformNode* FirstChild;
		DyTransformNode* NextSibling;
		DyTransformNode* PrevSibling;
		DyTransformNode* Parent;
		int32_t ChildCount = 0;
	};

	DyTransformNode::DyTransformNode(const char* id)
		:Impl(new TransformNodeP())
	{
		Impl->Id = id;
	}

	DyTransformNode::~DyTransformNode()
	{
		delete Impl;
	}

	void DyTransformNode::AddChildNode(DyTransformNode* Child)
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

}