#pragma once
#include "Scene/CameraComponent.h"
namespace Engine
{
	struct ModelPreviewCameraComponentP;

	class ModelPreviewCameraComponent final : public CameraComponent
	{
	public:
		ModelPreviewCameraComponent(std::weak_ptr<Actor> Owner);
		virtual ~ModelPreviewCameraComponent();
	};
}