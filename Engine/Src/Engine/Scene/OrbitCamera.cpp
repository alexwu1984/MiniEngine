#include "Scene/OrbitCamera.h"
#include "math/quaternion.h"
#include "math/matrix4x4.h"
#include "Scene/Actor.h"

namespace Engine
{
	using namespace math;
	struct OrbitCameraP
	{
		// Offset from target
		Vector3 Offset;
		// Up vector of camera
		Vector3 Up;
		// Rotation/sec speed of pitch
		float PitchSpeed = 0.f;
		// Rotation/sec speed of yaw
		float YawSpeed = 0.f;
	};

	OrbitCamera::OrbitCamera(std::weak_ptr<Actor> Owner)
		:CameraComponent(Owner)
		, Impl(std::make_shared<OrbitCameraP>())
	{
		Impl->Up = Vector3::UnitZ;
		
	}

	OrbitCamera::~OrbitCamera()
	{

	}

	void OrbitCamera::Tick(float DeltaTime)
	{
		CameraComponent::Tick(DeltaTime);

		Impl->Offset = GetCameraPos();
		// Transform offset and up by yaw
		Quaternion Yaw(Vector3::UnitY, Impl->YawSpeed * DeltaTime);
		
		Impl->Offset = Vector3::Transform(Impl->Offset, Yaw);
		Impl->Up = Vector3::Transform(Impl->Up, Yaw);

		Vector3 forward = Impl->Offset;
		forward.z *= -1;
		forward = forward.Normalize();
		Vector3 right = Vector3::Cross(Impl->Up, forward);
		right = right.Normalize();

		// Transform camera offset and up by pitch
		Quaternion Pitch(right, Impl->PitchSpeed * DeltaTime);
		Impl->Offset = Vector3::Transform(Impl->Offset, Pitch);
		Impl->Up = Vector3::Transform(Impl->Up, Pitch);

		Vector3 Target = GetOwner()->GetPosition();
		Vector3 CameraPos = Target + Impl->Offset;
		Matrix4x4 View = Matrix4x4::MatrixLookAtLH(CameraPos, Target, Impl->Up);
		SetViewMatrix(View);

		UpdateFrustum(CameraPos, forward.Normalize(), Impl->Up);
	}

	void OrbitCamera::InitResource()
	{
		CameraComponent::InitResource();
		
	}

	float OrbitCamera::GetPitchSpeed() const
	{
		return Impl->PitchSpeed;
	}

	float OrbitCamera::GetYawSpeed() const
	{
		return Impl->YawSpeed;
	}

	void OrbitCamera::SetPitchSpeed(float Speed)
	{
		Impl->PitchSpeed = Speed;
	}

	void OrbitCamera::SetYawSpeed(float Speed)
	{
		Impl->YawSpeed = Speed;
	}


}

