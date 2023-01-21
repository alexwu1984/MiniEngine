#pragma once
#include "Scene/Component.h"

namespace Engine
{
	struct GltfMeshComponentP;
	class GltfMesh;

	class GltfMeshComponent : public Component
	{
	public:
		GltfMeshComponent(class std::weak_ptr<Actor> Owner);
		~GltfMeshComponent();

		bool Load(const std::wstring& FileName);

		virtual void Draw(RHICommandContext& RHIContext, std::shared_ptr<CameraComponent> Camera);

	private:
		std::shared_ptr< GltfMeshComponentP> Impl;
	};
}

