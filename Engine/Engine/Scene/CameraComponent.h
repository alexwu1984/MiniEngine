#pragma once
#include "Scene/Component.h"
#include "math/matrix4x4.h"
#include "math/frustum.h"

namespace Engine
{
	struct CameraComponentPrivate;

	class CameraComponent : public Component
	{
	public:
		DECLARE_COMPONENT_CLASS_NAME(CameraComponent)
		CameraComponent(std::weak_ptr<Actor> Owner);
		virtual ~CameraComponent();

		virtual void InitResource() override;
		virtual void Tick(float DeltaTime) override;

		void SetViewMatrix(const math::Matrix4x4& view);
		void SetCameraPos(const math::Vector3& Pos);

		math::Matrix4x4 GetViewMatrix()const;
		math::Matrix4x4 GetPrevViewMatrix()const;
		math::Vector3 GetCameraPos() const;
	 
		math::Matrix4x4 GetProjMatrix() const;
		math::Vector4 GetTemporalAAJitter() const;

		void UpdateFrustum(const math::Vector3& eye, const math::Vector3& forward, const math::Vector3& up);
		const math::Frustum& GetFrustum() const;

		void SetProjectionJitter(uint32_t width, uint32_t height, uint32_t& sampleIndex);

		math::Matrix4x4 HackAddTemporalAAProjectionJitter( bool PrevFrame = false);

	protected:
		CameraComponentPrivate* d_ptr;
	};
	DECLARE_COMPONENT_TRAITS_CLASS_NAME(CameraComponent);
}