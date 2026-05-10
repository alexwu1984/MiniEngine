#include "Scene/CameraComponent.h"
#include "Scene/CameraComponentPrivate.h"
#include "Engine/Engine.h"
#include "App/AppWindow.h"
#include "Scene/Actor.h"
#include "math/vector2.h"
#include <cmath>

namespace Engine
{
	namespace
	{
		/** UE4 default r.TemporalAASamples == 8: Halton(2,3) subpixel jitter cycle. */
		static constexpr int32_t kTemporalAASampleCount = 8;

		void SnapTemporalPrevToCurrent(CameraComponentPrivate* d)
		{
			d->PreviousView = d->View;
			d->PrevProjMatrix = d->ProjMatrix;
			d->PrevjitterX = d->jitterX;
			d->PrevjitterY = d->jitterY;
		}
	} // namespace

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
	}

	void CameraComponent::Tick(float DeltaTime)
	{
		C_P(CameraComponent);

		// Compute new camera from this actor
		math::Vector3 CameraPos = GetCameraPos();
		math::Vector3 Target = d->bExplicitLookAtWorld ? d->ExplicitLookAtWorld : Vector3::UnitZ;
		math::Vector3 Up = math::Vector3::UnitY;

		math::Matrix4x4 ViewMatrix = math::Matrix4x4::MatrixLookAtLH(CameraPos, Target, Up);
		SetViewMatrix(ViewMatrix);
		math::Vector3 ViewForward = Target - CameraPos;
		const float vfLenSq = ViewForward.GetSqrLength();
		if (vfLenSq < 1e-12f)
			ViewForward = Vector3::UnitZ;
		else
			ViewForward *= 1.f / std::sqrt(vfLenSq);
		// UpdateFrustum expects the negated view direction (matches legacy UnitZ when Target was origin-ward).
		UpdateFrustum(CameraPos, ViewForward * -1.f, Up);
		
		auto AppWin = GEngine->GetAppWindow();
		auto Width = AppWin->GetWidth();
		auto Height = AppWin->GetHeight();
		uint32_t UnusedProjectionJitterSampleArg = 0;
		SetProjectionJitter(Width, Height, UnusedProjectionJitterSampleArg);

		// Heuristic camera cut: large world-space jump in one tick (gameplay teleport / scene swap).
		static constexpr float kTemporalAATeleportMeters = 30.f;
		const float kSq = kTemporalAATeleportMeters * kTemporalAATeleportMeters;
		const math::Vector3 CameraPosAfter = d->CameraPos;
		if (d->TemporalHistoryHasLastPos)
		{
			const float dx = CameraPosAfter.x - d->TemporalHistoryLastPos.x;
			const float dy = CameraPosAfter.y - d->TemporalHistoryLastPos.y;
			const float dz = CameraPosAfter.z - d->TemporalHistoryLastPos.z;
			if (dx * dx + dy * dy + dz * dz > kSq)
				NotifyTemporalHistoryInvalidate();
		}
		d->TemporalHistoryLastPos = CameraPosAfter;
		d->TemporalHistoryHasLastPos = true;

		EnsureTemporalPrevMatricesInitialized();
	}

	void CameraComponent::EnsureTemporalPrevMatricesInitialized()
	{
		C_P(CameraComponent);
		if (!d->bTemporalPrevMatricesValid)
		{
			SnapTemporalPrevToCurrent(d);
			d->bTemporalPrevMatricesValid = true;
		}
	}

	void CameraComponent::NotifyTemporalHistoryInvalidate()
	{
		C_P(CameraComponent);
		++d->TemporalHistoryGeneration;
		SnapTemporalPrevToCurrent(d);
	}

	void CameraComponent::MarkTemporalHistoryStaleAfterSceneCut()
	{
		C_P(CameraComponent);
		++d->TemporalHistoryGeneration;
		d->bTemporalPrevMatricesValid = false;
		// New scene: ignore roam/teleport heuristic until we have stable positions in this world (BS Roam -> fixed Model3).
		d->TemporalHistoryHasLastPos = false;
		d->TemporalHistoryLastPos = math::Vector3(0.f, 0.f, 0.f);
		d->FrameIndex = 0u;
		d->FrameIndexMod2 = 0u;
		d->jitterX = d->jitterY = d->PrevjitterX = d->PrevjitterY = 0.f;
	}

	uint32_t CameraComponent::GetTemporalHistoryGeneration() const
	{
		C_P(const CameraComponent);
		return d->TemporalHistoryGeneration;
	}

	void CameraComponent::SetViewMatrix(const math::Matrix4x4& view)
	{
		C_P(CameraComponent);
		auto AppWin = GEngine->GetAppWindow();
		d->PreviousView = d->View;
		d->View = view;
		d->Aspect = (float)AppWin->GetWidth() / (float)AppWin->GetHeight();
		d->PrevProjMatrix = d->ProjMatrix;
		d->ProjMatrix = Matrix4x4::MatrixPerspectiveFovLH(d->FovVertical, d->Aspect, d->Near, d->Far);
	}

	Matrix4x4 CameraComponent::GetViewMatrix() const
	{
		C_P(const CameraComponent);
		return d->View;
	}

	Matrix4x4 CameraComponent::GetPrevViewMatrix() const
	{
		C_P(const CameraComponent);
		return d->PreviousView;
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

	void CameraComponent::SetExplicitLookAtWorldTarget(const math::Vector3& worldLookAt, bool bEnable)
	{
		C_P(CameraComponent);
		d->ExplicitLookAtWorld = worldLookAt;
		d->bExplicitLookAtWorld = bEnable;
	}

	float CameraComponent::GetFovVerticalRadians() const
	{
		C_P(const CameraComponent);
		return d->FovVertical;
	}

	math::Matrix4x4 CameraComponent::GetPrevProjMatrix() const
	{
		C_P(const CameraComponent);
		return d->PrevProjMatrix;
	}

	Matrix4x4 CameraComponent::GetProjMatrix() const
	{
		C_P(const CameraComponent);
		return d->ProjMatrix;
	}

	float CameraComponent::GetNearPlane() const
	{
		C_P(const CameraComponent);
		return d->Near;
	}

	float CameraComponent::GetFarPlane() const
	{
		C_P(const CameraComponent);
		return d->Far;
	}

	Vector4 CameraComponent::GetTemporalAAJitter() const
	{
		C_P(const CameraComponent);
		return Vector4(d->jitterX, d->jitterY, d->PrevjitterX, d->PrevjitterY);
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

	/** [ Halton 1964, "Radical-inverse quasi-random point sequence" ] */
	inline float Halton(int32_t Index, int32_t Base)
	{
		float Result = 0.0f;
		float InvBase = 1.0f / Base;
		float Fraction = InvBase;
		while (Index > 0)
		{
			Result += (Index % Base) * Fraction;
			Index /= Base;
			Fraction *= InvBase;
		}
		return Result;
	}


	void CameraComponent::SetProjectionJitter(uint32_t width, uint32_t height, uint32_t& sampleIndex)
	{
		C_P(CameraComponent);
		d->PrevjitterX = d->jitterX;
		d->PrevjitterY = d->jitterY;

		d->FrameIndex++;
		d->FrameIndexMod2 = d->FrameIndex % 2;

		// +1 skips Halton(0,*) degenerate origin; index wraps 1..kTemporalAASampleCount (UE-style 8-tap cycle).
		const int32_t JitterIndex = static_cast<int32_t>((d->FrameIndex - 1) % kTemporalAASampleCount) + 1;
		const float JitterX = Halton(JitterIndex, 2) - 0.5f;
		const float JitterY = Halton(JitterIndex, 3) - 0.5f;

		d->jitterX = JitterX * 2.f / static_cast<float>(width);
		d->jitterY = JitterY * -2.f / static_cast<float>(height);
	}

	math::Matrix4x4 CameraComponent::HackAddTemporalAAProjectionJitter(bool PrevFrame /*= false*/)
	{
		C_P(CameraComponent);
		math::Matrix4x4 ProjectMatrix;
		math::Vector2 TemporalAAProjectionJitter;
		if (PrevFrame)
		{
			ProjectMatrix = d->PrevProjMatrix;
			TemporalAAProjectionJitter.x = d->PrevjitterX;
			TemporalAAProjectionJitter.y = d->PrevjitterY;
		}
		else
		{
			ProjectMatrix = d->ProjMatrix;
			TemporalAAProjectionJitter.x = d->jitterX;
			TemporalAAProjectionJitter.y = d->jitterY;
		}

		ProjectMatrix.r2[0] += TemporalAAProjectionJitter.x;
		ProjectMatrix.r2[1] += TemporalAAProjectionJitter.y;

		return ProjectMatrix;
	}

	int32_t CameraComponent::GetFrameIndexMod2() const
	{
		C_P(const CameraComponent);
		return d->FrameIndexMod2;
	}

	int32_t CameraComponent::GetFrameIndex() const
	{
		C_P(const CameraComponent);
		return d->FrameIndex;
	}

}