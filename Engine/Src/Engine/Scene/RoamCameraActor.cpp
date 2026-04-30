#include "Scene/RoamCameraActor.h"
#include "Scene/FreeRoamCameraComponent.h"
#include "Scene/GltfInputComponent.h"
#include "Scene/World.h"
#include "math/vector3.h"

namespace Engine
{
	IMP_ACTOR_CLASS_NAME(RoamCameraActor)
	IMP_ACTOR_TRAITS_CLASS_NAME(RoamCameraActor)

	RoamCameraActor::RoamCameraActor(std::weak_ptr<World> InWorld, const nlohmann::json& InRoamJson)
		: Actor(std::move(InWorld))
		, RoamJson(InRoamJson)
	{
	}

	RoamCameraActor::~RoamCameraActor() = default;

	void RoamCameraActor::InitResouce()
	{
		Actor::InitResouce();
		auto cam = std::make_shared<FreeRoamCameraComponent>(shared_from_this());
		cam->InitResource();

		try
		{
			if (RoamJson.find("MoveSpeed") != RoamJson.end() && RoamJson["MoveSpeed"].is_number())
				cam->SetMoveSpeed(static_cast<float>(RoamJson["MoveSpeed"].get<double>()));
			if (RoamJson.find("LookSensitivity") != RoamJson.end() && RoamJson["LookSensitivity"].is_number())
				cam->SetLookSensitivity(static_cast<float>(RoamJson["LookSensitivity"].get<double>()));
		}
		catch (const std::exception&)
		{
		}

		if (RoamJson.find("Position") != RoamJson.end() && RoamJson["Position"].is_string())
		{
			const std::string posStr = RoamJson["Position"].get<std::string>();
			math::Vector3 pos{};
			std::sscanf(posStr.c_str(), "%f,%f,%f", &pos.x, &pos.y, &pos.z);
			SetPosition(pos);
		}

		cam->SetCameraPos(GetPosition());

		if (RoamJson.find("LookAt") != RoamJson.end() && RoamJson["LookAt"].is_string())
		{
			const std::string atStr = RoamJson["LookAt"].get<std::string>();
			math::Vector3 at{};
			std::sscanf(atStr.c_str(), "%f,%f,%f", &at.x, &at.y, &at.z);
			cam->SetInitialLookToward(GetPosition(), at);
		}

		try
		{
			float yawOff = 0.f, pitchOff = 0.f;
			if (RoamJson.find("LookYawOffsetDegrees") != RoamJson.end() && RoamJson["LookYawOffsetDegrees"].is_number())
				yawOff = static_cast<float>(RoamJson["LookYawOffsetDegrees"].get<double>());
			if (RoamJson.find("LookPitchOffsetDegrees") != RoamJson.end() && RoamJson["LookPitchOffsetDegrees"].is_number())
				pitchOff = static_cast<float>(RoamJson["LookPitchOffsetDegrees"].get<double>());
			if (yawOff != 0.f || pitchOff != 0.f)
				cam->AddInitialYawPitchOffset(yawOff, pitchOff);
		}
		catch (const std::exception&)
		{
		}

		try
		{
			if (RoamJson.find("MainCamera") != RoamJson.end() && RoamJson["MainCamera"].get<bool>())
			{
				if (const auto w = GetWorld())
					w->SetMainCamera(cam);
			}
		}
		catch (const std::exception&)
		{
		}

		AddComponent(cam);

		auto input = std::make_shared<GltfDeviceInputComponent>(shared_from_this());
		input->SetMouseRotateModelEnabled(false);
		input->InitResource();
		AddComponent(input);
	}

} // namespace Engine
