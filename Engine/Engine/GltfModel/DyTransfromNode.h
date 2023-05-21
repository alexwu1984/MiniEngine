#pragma once
#include "math/vector3.h"
#include "math/quaternion.h"
#include "math/matrix4x4.h"

namespace Engine
{
	struct TransformNodeP;

	class DyTransformNode
	{
	public:
		DyTransformNode(const char* id);
		~DyTransformNode();

		void AddChildNode(DyTransformNode* Child);

		void SetLocalPosition(const math::Vector3& Pos);
		void SetLocalRotation(const math::Quaternion& Rot);
		void SetLocalToWorld(const math::Matrix4x4& Mat);
		void SetWorldToLocal(const math::Matrix4x4& Mat);

	private:
		TransformNodeP* Impl = nullptr;
	};
}

