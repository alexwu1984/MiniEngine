#include "Scene/FreeRoamCameraComponent.h"
#include "Scene/CameraComponentPrivate.h"
#include "Scene/DeviceInputState.h"
#include "Scene/Actor.h"
#include "Engine.h"
#include "App/AppWindow.h"
#include "math/math.h"

namespace Engine
{
	IMP_COMPONENT_CLASS_NAME(FreeRoamCameraComponent)
	IMP_COMPONENT_TRAITS_CLASS_NAME(FreeRoamCameraComponent)

	using namespace math;

	FreeRoamCameraComponent::FreeRoamCameraComponent(std::weak_ptr<Actor> Owner)
		: CameraComponent(std::move(Owner))
	{
	}

	void FreeRoamCameraComponent::InitResource()
	{
		CameraComponent::InitResource();
	}

	void FreeRoamCameraComponent::SetInitialLookToward(const Vector3& EyeWorld, const Vector3& TargetWorld)
	{
		Vector3 dir = TargetWorld - EyeWorld;
		if (dir.GetSqrLength() < 1e-12f)
			return;
		dir = dir.Normalize();
		const float pitchRad = std::asinf((std::min)(1.f, (std::max)(-1.f, dir.y)));
		const float cosPitch = std::cosf(pitchRad);
		if (std::fabsf(cosPitch) < 1e-5f)
		{
			YawDegrees = 0.f;
			PitchDegrees = math::Degrees(pitchRad);
			return;
		}
		const float yawRad = std::atan2f(dir.x, dir.z);
		YawDegrees = math::Degrees(yawRad);
		PitchDegrees = math::Degrees(pitchRad);
	}

	void FreeRoamCameraComponent::AddInitialYawPitchOffset(float yawDeg, float pitchDeg)
	{
		YawDegrees += yawDeg;
		PitchDegrees += pitchDeg;
		PitchDegrees = (std::min)(85.f, (std::max)(-85.f, PitchDegrees));
	}

	void FreeRoamCameraComponent::ApplyMouseLookPixelDelta(float Dx, float Dy)
	{
		YawDegrees += Dx * LookSensitivity;
		PitchDegrees -= Dy * LookSensitivity;
		PitchDegrees = (std::min)(85.f, (std::max)(-85.f, PitchDegrees));
	}

	void FreeRoamCameraComponent::ApplyKeyboardNavigation(const KeyboardFrameInput& Keys, float DeltaTime)
	{
		const float yawRad = Radians(YawDegrees);
		const float pitchRad = Radians(PitchDegrees);
		Vector3 forward(cosf(pitchRad) * sinf(yawRad), sinf(pitchRad), cosf(pitchRad) * cosf(yawRad));
		forward = forward.Normalize();

		Vector3 right = Vector3::Cross(Vector3::UnitY, forward);
		if (right.GetSqrLength() < 1e-8f)
			right = Vector3::Cross(Vector3(0.f, 0.f, 1.f), forward);
		right = right.Normalize();

		Vector3 eye = GetCameraPos();
		const float step = MoveSpeed * DeltaTime;
		if (Keys.bW)
			eye = eye + forward * step;
		if (Keys.bS)
			eye = eye - forward * step;
		if (Keys.bA)
			eye = eye - right * step;
		if (Keys.bD)
			eye = eye + right * step;
		if (Keys.bSpace)
			eye = eye + Vector3::UnitY * step;
		if (Keys.bCtrl)
			eye = eye - Vector3::UnitY * step;

		SetCameraPos(eye);
		if (const auto owner = GetOwner())
			owner->SetPosition(eye);
	}

	void FreeRoamCameraComponent::ApplyWheelZoom(int32_t WheelNotches)
	{
		if (WheelNotches == 0)
			return;
		// Win32 WM_MOUSEWHEEL reports delta in multiples of 120 (WHEEL_DELTA), not ±1 per detent.
		static constexpr float kWheelDelta = 120.f;
		static constexpr float kZoomWorldUnitsPerDetent = 0.12f;
		const float detents = static_cast<float>(WheelNotches) / kWheelDelta;
		const float yawRad = Radians(YawDegrees);
		const float pitchRad = Radians(PitchDegrees);
		Vector3 forward(cosf(pitchRad) * sinf(yawRad), sinf(pitchRad), cosf(pitchRad) * cosf(yawRad));
		forward = forward.Normalize();
		Vector3 eye = GetCameraPos() + forward * (kZoomWorldUnitsPerDetent * detents);
		SetCameraPos(eye);
		if (const auto owner = GetOwner())
			owner->SetPosition(eye);
	}

	void FreeRoamCameraComponent::Tick(float DeltaTime)
	{
		(void)DeltaTime;
		C_P(CameraComponent);

		const float yawRad = Radians(YawDegrees);
		const float pitchRad = Radians(PitchDegrees);
		Vector3 forward(cosf(pitchRad) * sinf(yawRad), sinf(pitchRad), cosf(pitchRad) * cosf(yawRad));
		forward = forward.Normalize();

		const Vector3 eye = GetCameraPos();
		const Vector3 at = eye + forward;
		const Matrix4x4 view = Matrix4x4::MatrixLookAtLH(eye, at, Vector3::UnitY);
		SetViewMatrix(view);
		UpdateFrustum(eye, forward, Vector3::UnitY);

		auto AppWin = GEngine->GetAppWindow();
		const uint32_t Width = AppWin->GetWidth();
		const uint32_t Height = AppWin->GetHeight();
		SetProjectionJitter(Width, Height);

		static constexpr float kTemporalAATeleportMeters = 30.f;
		const float kSq = kTemporalAATeleportMeters * kTemporalAATeleportMeters;
		const Vector3 CameraPosAfter = d->CameraPos;
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

} // namespace Engine
