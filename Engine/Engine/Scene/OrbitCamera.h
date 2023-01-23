#pragma once
#include "Scene/CameraComponent.h"

namespace Engine
{
	struct OrbitCameraP;

	class OrbitCamera final: public CameraComponent
	{
	public:
		OrbitCamera(std::weak_ptr<Actor> Owner);
		virtual ~OrbitCamera();

		void Tick(float DeltaTime) override;
		void InitResource() override;

		float GetPitchSpeed() const;
		float GetYawSpeed() const;

		void SetPitchSpeed(float Speed);
		void SetYawSpeed(float Speed);
	private:
		std::shared_ptr< OrbitCameraP> Impl;
	};
}