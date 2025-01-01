#pragma once
#include "math/vector3.h"
#include "math/quaternion.h"
#include "math/matrix4x4.h"

namespace Engine
{
	struct TransformNodePrivate;

	class FDyTransformNode
	{
	public:
		FDyTransformNode(const char* id);
		~FDyTransformNode();

		void AddChildNode(FDyTransformNode* Child);

		void SetLocalPosition(const math::Vector3& Pos);
		void SetWorldPosition(const math::Vector3& Pos);
		void SetLocalRotation(const math::Quaternion& Rot);
		void SetWorldRotation(const math::Quaternion& Rot);
		void SetLocalToWorld(const math::Matrix4x4& Mat);
		void SetWorldToLocal(const math::Matrix4x4& Mat);

		math::Vector3 GetLocalPosition() const;
		math::Vector3 GetWorldPosition() const;
		math::Quaternion GetLocalRotation() const;
		math::Quaternion GetWorldRotation() const;
		math::Matrix4x4 GetWorldToLocal() const;
		math::Matrix4x4 GetLocalToWorld() const;

		FDyTransformNode* GetFirstChild() const;
		int32_t GetChildCount() const;

		std::string GetID() const;

	private:
		TransformNodePrivate* Impl = nullptr;
	};
}

