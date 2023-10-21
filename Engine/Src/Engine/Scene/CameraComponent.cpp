#include "Scene/CameraComponent.h"
#include "Scene/CameraComponentPrivate.h"
#include "Engine/Engine.h"
#include "App/AppWindow.h"
#include "Scene/Actor.h"
#include "Scene/SceneView.h"

namespace Engine
{
	IMP_COMPONENT_CLASS_NAME(CameraComponent)
	IMP_COMPONENT_TRAITS_CLASS_NAME(CameraComponent)

	using namespace math;
	CameraComponent::CameraComponent(std::weak_ptr<Actor> Owner)
		:Component(Owner)
		,d_ptr(new CameraComponentPrivate)
	{

	}

	CameraComponent::~CameraComponent()
	{
		delete d_ptr;
	}

	void CameraComponent::InitResource()
	{
		Component::InitResource();
		GetOwner()->GetScene()->SetMainCamera(std::static_pointer_cast<CameraComponent>(shared_from_this()));
	}

	void CameraComponent::Tick(float DeltaTime)
	{
		// Compute new camera from this actor
		math::Vector3 CameraPos = GetCameraPos();
		math::Vector3 Target =  Vector3::UnitZ;
		math::Vector3 Up = math::Vector3::UnitY;

		math::Matrix4x4 ViewMatrix = math::Matrix4x4::MatrixLookAtLH(CameraPos, Target, Up);
		SetViewMatrix(ViewMatrix);
		UpdateFrustum(CameraPos, Vector3(Vector3::UnitZ).Normalize(), Up);
	}

	void CameraComponent::SetViewMatrix(const math::Matrix4x4& view)
	{
		C_P(CameraComponent);
		const auto& AppWin = GEngine->GetAppWindow();
		d->View = view;
		d->Aspect = (float)AppWin->GetWidth() / (float)AppWin->GetHeight();
		d->ProjMatrix = Matrix4x4::MatrixPerspectiveFovLH(d->FovVertical, d->Aspect, d->Near, d->Far);
	}

	Matrix4x4 CameraComponent::GetViewMatrix() const
	{
		C_P(const CameraComponent);
		return d->View;
	}

	Vector3 CameraComponent::GetCameraPos() const
	{
		C_P(const CameraComponent);
		return d->CameraPos;
	}

	void CameraComponent::SetCameraPos(const math::Vector3& Pos)
	{
		C_P(CameraComponent);
		d->CameraPos = Pos;
	}

	Matrix4x4 CameraComponent::GetProjMatrix() const
	{
		C_P(const CameraComponent);
		return d->ProjMatrix;
	}

	void CameraComponent::UpdateFrustum(const math::Vector3& eye, const math::Vector3& forward, const math::Vector3& up)
	{
		C_P(CameraComponent);
		Vector3 side = Vector3::Cross(up, forward);
		side.Normalize();

		float near_height_half = d->Near * std::tan(d->FovVertical / 2.f);
		float far_height_half = d->Far * std::tan(d->FovVertical / 2.f);
		float near_width_half = near_height_half * d->Aspect;
		float far_width_half = far_height_half * d->Aspect;

		// near plane
		Vector3 near_center = eye - forward * d->Near;
		Vector3 near_normal = forward * -1.f;
		d->Frustum.planes[0].Set(near_normal, near_center);

		// far plane
		Vector3 far_center = eye + forward * d->Far;
		Vector3 far_normal = forward;
		d->Frustum.planes[1].Set(far_normal, far_center);

		// top plane
		Vector3 top_center = near_center + up * near_height_half;
		Vector3 top_normal = Vector3::Cross((top_center - eye).Normalize(), side);
		d->Frustum.planes[2].Set(top_normal, top_center);

		// bottom plane
		Vector3 bottom_center = near_center - up * near_height_half;
		Vector3 bottom_normal = Vector3::Cross(side, (bottom_center - eye).Normalize());
		d->Frustum.planes[3].Set(bottom_normal, bottom_center);

		// left plane
		Vector3 left_center = near_center - side * near_width_half;
		Vector3 left_normal = Vector3::Cross((left_center - eye).Normalize(), up);
		d->Frustum.planes[4].Set(left_normal, left_center);

		// right plane
		Vector3 right_center = near_center + side * near_width_half;
		Vector3 right_normal = Vector3::Cross(up, (right_center - eye).Normalize());
		d->Frustum.planes[5].Set(right_normal, right_center);

		// 8 corners
		Vector3 nearTopLeft = near_center + up * near_height_half - side * near_width_half;
		Vector3 nearTopRight = near_center + up * near_height_half + side * near_width_half;
		Vector3 nearBottomLeft = near_center - up * near_height_half - side * near_width_half;
		Vector3 nearBottomRight = near_center - up * near_height_half + side * near_width_half;

		Vector3 farTopLeft = far_center + up * far_height_half - side * far_width_half;
		Vector3 farTopRight = far_center + up * far_height_half + side * far_width_half;
		Vector3 farBottomLeft = far_center - up * far_height_half - side * far_width_half;
		Vector3 farBottomRight = far_center - up * far_height_half + side * far_width_half;

		d->Frustum.corners[0] = nearTopLeft;
		d->Frustum.corners[1] = nearTopRight;
		d->Frustum.corners[2] = nearBottomLeft;
		d->Frustum.corners[3] = nearBottomRight;
		d->Frustum.corners[4] = farTopLeft;
		d->Frustum.corners[5] = farTopRight;
		d->Frustum.corners[6] = farBottomLeft;
		d->Frustum.corners[7] = farBottomRight;

		d->Frustum.bbox.Set(Vector3::Zero, Vector3::Zero);

		for (auto& corner : d->Frustum.corners)
		{
			d->Frustum.bbox.UpdateMinMax(corner);
		}
	}

	const math::Frustum& CameraComponent::GetFrustum() const
	{
		C_P(const CameraComponent);
		return d->Frustum;
	}

}