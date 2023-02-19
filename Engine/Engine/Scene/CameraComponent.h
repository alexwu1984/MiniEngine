#pragma once
#include "Scene/Component.h"
#include "math/matrix4x4.h"
#include "math/frustum.h"

namespace Engine
{
	struct CameraComponentP;

	class CameraComponent : public Component
	{
	public:
		CameraComponent(std::weak_ptr<Actor> Owner);
		virtual ~CameraComponent();

		virtual void InitResource() override;
		virtual void Tick(float DeltaTime) override;

		void SetViewMatrix(const math::Matrix4x4& view);
		void SetCameraPos(const math::Vector3& Pos);

		math::Matrix4x4 GetViewMatrix()const;
		math::Vector3 GetCameraPos() const;
	 
		math::Matrix4x4 GetProjMatrix() const;

		void UpdateFrustum(const math::Vector3& eye, const math::Vector3& forward, const math::Vector3& up);
		const math::Frustum& GetFrustum() const;

	protected:
		std::shared_ptr< CameraComponentP> ImplCameraP;
	};
}