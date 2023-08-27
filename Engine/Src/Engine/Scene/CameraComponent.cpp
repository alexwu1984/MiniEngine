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
		, ImplCameraP(std::make_shared<CameraComponentP>())
	{

	}

	CameraComponent::~CameraComponent()
	{

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
		const auto& AppWin = GEngine->GetAppWindow();
		ImplCameraP->View = view;
		ImplCameraP->Aspect = (float)AppWin->GetWidth() / (float)AppWin->GetHeight();
		ImplCameraP->ProjMatrix = Matrix4x4::MatrixPerspectiveFovLH(ImplCameraP->FovVertical, ImplCameraP->Aspect, ImplCameraP->Near, ImplCameraP->Far);
	}

	Matrix4x4 CameraComponent::GetViewMatrix() const
	{
		return ImplCameraP->View;
	}

	Vector3 CameraComponent::GetCameraPos() const
	{
		return ImplCameraP->CameraPos;
	}

	void CameraComponent::SetCameraPos(const math::Vector3& Pos)
	{
		ImplCameraP->CameraPos = Pos;
	}

	Matrix4x4 CameraComponent::GetProjMatrix() const
	{
		return ImplCameraP->ProjMatrix;
	}

	void CameraComponent::UpdateFrustum(const math::Vector3& eye, const math::Vector3& forward, const math::Vector3& up)
	{
		Vector3 side = Vector3::Cross(up, forward);
		side.Normalize();

		float near_height_half = ImplCameraP->Near * std::tan(ImplCameraP->FovVertical / 2.f);
		float far_height_half = ImplCameraP->Far * std::tan(ImplCameraP->FovVertical / 2.f);
		float near_width_half = near_height_half * ImplCameraP->Aspect;
		float far_width_half = far_height_half * ImplCameraP->Aspect;

		// near plane
		Vector3 near_center = eye - forward * ImplCameraP->Near;
		Vector3 near_normal = forward * -1.f;
		ImplCameraP->Frustum.planes[0].Set(near_normal, near_center);

		// far plane
		Vector3 far_center = eye + forward * ImplCameraP->Far;
		Vector3 far_normal = forward;
		ImplCameraP->Frustum.planes[1].Set(far_normal, far_center);

		// top plane
		Vector3 top_center = near_center + up * near_height_half;
		Vector3 top_normal = Vector3::Cross((top_center - eye).Normalize(), side);
		ImplCameraP->Frustum.planes[2].Set(top_normal, top_center);

		// bottom plane
		Vector3 bottom_center = near_center - up * near_height_half;
		Vector3 bottom_normal = Vector3::Cross(side, (bottom_center - eye).Normalize());
		ImplCameraP->Frustum.planes[3].Set(bottom_normal, bottom_center);

		// left plane
		Vector3 left_center = near_center - side * near_width_half;
		Vector3 left_normal = Vector3::Cross((left_center - eye).Normalize(), up);
		ImplCameraP->Frustum.planes[4].Set(left_normal, left_center);

		// right plane
		Vector3 right_center = near_center + side * near_width_half;
		Vector3 right_normal = Vector3::Cross(up, (right_center - eye).Normalize());
		ImplCameraP->Frustum.planes[5].Set(right_normal, right_center);

		// 8 corners
		Vector3 nearTopLeft = near_center + up * near_height_half - side * near_width_half;
		Vector3 nearTopRight = near_center + up * near_height_half + side * near_width_half;
		Vector3 nearBottomLeft = near_center - up * near_height_half - side * near_width_half;
		Vector3 nearBottomRight = near_center - up * near_height_half + side * near_width_half;

		Vector3 farTopLeft = far_center + up * far_height_half - side * far_width_half;
		Vector3 farTopRight = far_center + up * far_height_half + side * far_width_half;
		Vector3 farBottomLeft = far_center - up * far_height_half - side * far_width_half;
		Vector3 farBottomRight = far_center - up * far_height_half + side * far_width_half;

		ImplCameraP->Frustum.corners[0] = nearTopLeft;
		ImplCameraP->Frustum.corners[1] = nearTopRight;
		ImplCameraP->Frustum.corners[2] = nearBottomLeft;
		ImplCameraP->Frustum.corners[3] = nearBottomRight;
		ImplCameraP->Frustum.corners[4] = farTopLeft;
		ImplCameraP->Frustum.corners[5] = farTopRight;
		ImplCameraP->Frustum.corners[6] = farBottomLeft;
		ImplCameraP->Frustum.corners[7] = farBottomRight;

		ImplCameraP->Frustum.bbox.Set(Vector3::Zero, Vector3::Zero);

		for (auto& corner : ImplCameraP->Frustum.corners)
		{
			ImplCameraP->Frustum.bbox.UpdateMinMax(corner);
		}
	}

	const math::Frustum& CameraComponent::GetFrustum() const
	{
		return ImplCameraP->Frustum;
	}

}