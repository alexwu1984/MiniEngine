#include "Scene/GltfActor.h"
#include "Scene/CameraComponent.h"
#include "Scene/SceneMeshComponent.h"
#include "Scene/GltfInputComponent.h"
#include "Scene/World.h"
#include "GltfModel/GltfModel.h"
#include "core/strings.h"
#include <algorithm>
#include <cmath>

namespace Engine
{
	namespace
	{
		static void ApplyActorDisplayNameFromSceneJson(Actor& ActorRef, const nlohmann::json& GltfJson)
		{
			try
			{
				if (GltfJson.find("ActorName") != GltfJson.end() && GltfJson["ActorName"].is_string())
				{
					ActorRef.SetActorName(core::u8_ucs2(GltfJson["ActorName"].get<std::string>()));
					return;
				}
				if (GltfJson.find("Model") != GltfJson.end() && GltfJson["Model"].is_string())
				{
					std::string m = GltfJson["Model"].get<std::string>();
					const size_t slash = m.find_last_of("/\\");
					if (slash != std::string::npos)
						m = m.substr(slash + 1);
					const size_t dot = m.find_last_of('.');
					if (dot != std::string::npos)
						m = m.substr(0, dot);
					if (!m.empty())
						ActorRef.SetActorName(core::u8_ucs2(m));
					return;
				}
				if (GltfJson.find("ProceduralFloor") != GltfJson.end())
					ActorRef.SetActorName(L"ProceduralFloor");
			}
			catch (const std::exception&)
			{
			}
		}
	} // namespace

	IMP_ACTOR_CLASS_NAME(GltfActor)
	IMP_ACTOR_TRAITS_CLASS_NAME(GltfActor)

	struct GltfActorPrivate
	{
		nlohmann::json GltfJson;
		std::shared_ptr<CameraComponent> CameraComp;
		std::shared_ptr<SceneMeshComponent> MeshComp;
		std::shared_ptr<GltfDeviceInputComponent> InputComp;
	};

	GltfActor::GltfActor(std::weak_ptr<World> InWorld, const nlohmann::json& GltfJson)
		: Actor(InWorld)
		, d_ptr(new GltfActorPrivate())
	{
		C_P(GltfActor);
		d->GltfJson = GltfJson;
	}

	GltfActor::~GltfActor()
	{
		delete d_ptr;
	}

	void GltfActor::InitResouce()
	{
		Actor::InitResouce();
		C_P(GltfActor);
		ApplyActorDisplayNameFromSceneJson(*this, d->GltfJson);
		auto GetWorldPin = [this]() -> std::shared_ptr<World> { return GetWorld(); };
		d->MeshComp = std::make_shared<SceneMeshComponent>(this->shared_from_this());
		bool bLoad = d->MeshComp->Load(d->GltfJson);
		if (!bLoad)
		{
			return;
		}
		//SetActorName(d->MeshComp->GetModel().GetModelConfig()->GetModelName());

		AddComponent(d->MeshComp);
		d->CameraComp = std::make_shared<CameraComponent>(this->shared_from_this());
		d->CameraComp->InitResource();

		// Camera helpers (avoid duplicated box/framing/orbit computations).
		auto ComputeWorldCenterRadius = [&]() -> std::pair<math::Vector3, float>
		{
			ComputeWorldTransform(0.f);
			const math::Matrix4x4& W = GetWorldTransform();
			const math::AABB3 box = d->MeshComp ? d->MeshComp->GetModelBox() : math::AABB3{};
			const math::Vector3 localCenter = box.GetCenter();
			const math::Vector3 halfExtents = (box.GetMaxPoint() - box.GetMinPoint()) * 0.5f;
			float radius = halfExtents.GetLength();
			radius = std::max(radius, 1e-3f);
			const math::Vector3 worldCenter = W.TransformPosition(localCenter);
			return { worldCenter, radius };
		};

		auto ComputeOrbitDistanceForRadius = [&](float radius) -> float
		{
			const float fovY = d->CameraComp ? d->CameraComp->GetFovVerticalRadians() : (0.5f * math::MATH_PI);
			const float tanHalf = std::tan(fovY * 0.5f);
			static constexpr float kViewerFrameMargin = 1.18f;
			float dist = (radius / std::max(tanHalf, 1e-4f)) * kViewerFrameMargin;
			dist = std::max(dist, radius + 0.05f);
			return dist;
		};

		auto ApplyDefaultFramedCameraPose = [&]()
		{
			if (!d->CameraComp)
				return;
			const auto [worldCenter, radius] = ComputeWorldCenterRadius();
			const float dist = ComputeOrbitDistanceForRadius(radius);
			const math::Vector3 eye = worldCenter + math::Vector3(0.f, 0.f, dist);
			d->CameraComp->SetExplicitLookAtWorldTarget(worldCenter, true);
			d->CameraComp->SetCameraPos(eye);
		};

		// Apply transform before camera distance: GetModelBox() is local; actor Scale scales the mesh in world.
		if (d->GltfJson.find("Scale") != d->GltfJson.end())
		{
			SetScale(d->GltfJson["Scale"]);
		}

		if (d->GltfJson.find("Position") != d->GltfJson.end())
		{
			std::string PosStr = d->GltfJson["Position"];
			math::Vector3 Pos;
			std::sscanf(PosStr.c_str(), "%f,%f,%f", &Pos.x, &Pos.y, &Pos.z);
			SetPosition(Pos);
		}

		const bool bRoamScene = GetWorldPin() && GetWorldPin()->UsesRoamCameraScene();
		const bool bHasOrbitCameraJson = d->GltfJson.find("OrbitCamera") != d->GltfJson.end() && d->GltfJson["OrbitCamera"].is_object();

		// Non-roam scenes: this actor's camera is always the world main camera (viewer; no JSON MainCamera flag).
		if (!bRoamScene)
			if (auto w = GetWorldPin())
				w->SetMainCamera(std::static_pointer_cast<CameraComponent>(d->CameraComp));

		ComputeWorldTransform(0.f);
		const math::Matrix4x4& W = GetWorldTransform();

		bool bUsedJsonCameraPose = false;
		if (!bRoamScene && !bHasOrbitCameraJson && d->GltfJson.find("Camera") != d->GltfJson.end() && d->GltfJson["Camera"].is_object())
		{
			try
			{
				const auto& CamJ = d->GltfJson["Camera"];
				auto parseVec3 = [](const nlohmann::json& Ar, math::Vector3& Out) -> bool
				{
					if (!Ar.is_array() || Ar.size() < 3)
						return false;
					Out.x = Ar.at(0).get<float>();
					Out.y = Ar.at(1).get<float>();
					Out.z = Ar.at(2).get<float>();
					return true;
				};
				math::Vector3 LocalFrom;
				math::Vector3 LocalTo;
				if (CamJ.find("defaultFrom") != CamJ.end() && CamJ.find("defaultTo") != CamJ.end() && parseVec3(CamJ["defaultFrom"], LocalFrom)
					&& parseVec3(CamJ["defaultTo"], LocalTo))
				{
					const math::Vector3 WorldFrom = W.TransformPosition(LocalFrom);
					const math::Vector3 WorldTo = W.TransformPosition(LocalTo);
					d->CameraComp->SetExplicitLookAtWorldTarget(WorldTo, true);
					d->CameraComp->SetCameraPos(WorldFrom);
					bUsedJsonCameraPose = true;
				}
			}
			catch (const std::exception&)
			{
			}
		}

		if (!bRoamScene && !bHasOrbitCameraJson && !bUsedJsonCameraPose)
			ApplyDefaultFramedCameraPose();

		if (d->GltfJson.find("ProjShadow") != d->GltfJson.end())
		{
			d->MeshComp->SetProjectShadow(d->GltfJson["ProjShadow"]);
		}
		
		AddComponent(d->CameraComp);

		d->InputComp = std::make_shared<GltfDeviceInputComponent>(this->shared_from_this());
		d->InputComp->InitResource();

		auto EnableDefaultOrbitFromModelBounds = [&]()
		{
			if (!d->InputComp || !d->CameraComp)
				return;
			const auto [worldCenter, radius] = ComputeWorldCenterRadius();
			const float dist = ComputeOrbitDistanceForRadius(radius);
			d->InputComp->EnableOrbitCamera(true, worldCenter, dist, 0.f, 0.f);
			d->InputComp->SnapOrbitToCamera(d->CameraComp.get());
			d->InputComp->SetMouseRotateModelEnabled(false);
		};

		if (!bRoamScene)
		{
			if (bHasOrbitCameraJson)
			{
				try
				{
					const auto& O = d->GltfJson["OrbitCamera"];
					math::Vector3 tgt(0.f, 0.f, 0.f);
					if (O.find("target") != O.end() && O["target"].is_array() && O["target"].size() >= 3)
					{
						tgt.x = O["target"].at(0).get<float>();
						tgt.y = O["target"].at(1).get<float>();
						tgt.z = O["target"].at(2).get<float>();
					}
					float dist = 3.5f;
					if (O.find("distance") != O.end() && O["distance"].is_number())
						dist = static_cast<float>(O["distance"].get<double>());
					float yawDeg = 0.f;
					float pitchDeg = 0.f;
					if (O.find("yawDeg") != O.end() && O["yawDeg"].is_number())
						yawDeg = static_cast<float>(O["yawDeg"].get<double>());
					if (O.find("pitchDeg") != O.end() && O["pitchDeg"].is_number())
						pitchDeg = static_cast<float>(O["pitchDeg"].get<double>());
					static constexpr float kDegToRad = 3.14159265f / 180.f;
					d->InputComp->EnableOrbitCamera(true, tgt, dist, yawDeg * kDegToRad, pitchDeg * kDegToRad);
					d->InputComp->SnapOrbitToCamera(d->CameraComp.get());
					d->InputComp->SetMouseRotateModelEnabled(false);
				}
				catch (const std::exception&)
				{
					EnableDefaultOrbitFromModelBounds();
				}
			}
			else
			{
				EnableDefaultOrbitFromModelBounds();
			}
		}
		else
		{
			bool bMouseRotateModel = !GetWorldPin() || !GetWorldPin()->UsesRoamCameraScene();
			if (d->GltfJson.find("MouseRotateModel") != d->GltfJson.end() && !d->GltfJson["MouseRotateModel"].is_null())
				bMouseRotateModel = d->GltfJson["MouseRotateModel"].get<bool>();
			d->InputComp->SetMouseRotateModelEnabled(bMouseRotateModel);
		}
		AddComponent(d->InputComp);
	}

}