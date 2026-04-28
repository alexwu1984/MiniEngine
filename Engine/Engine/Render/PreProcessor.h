#pragma once
#include "core/inc.h"
#include "tinygltf/json.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
}

namespace Engine
{
	struct PreProcessorPrivate;
	class FSkyLightIBLPrecompute;

	class PreProcessor
	{
	public:
		PreProcessor(RenderCore::DynamicRHI* RHI);
		~PreProcessor();

		void InitResource();
		void LoadConfig(const nlohmann::json& Root);
		void Draw(RenderCore::RHICommandContext& RHIContext);

		std::shared_ptr<FSkyLightIBLPrecompute> GetIBLRender();

	private:
		PreProcessorPrivate* d_ptr = nullptr;
	};
}
