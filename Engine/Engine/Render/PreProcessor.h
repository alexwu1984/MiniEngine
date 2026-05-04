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

		std::shared_ptr<FSkyLightIBLPrecompute> GetSkyLightEnvironment();

		/** Call on render thread before PreProcess::Draw; applies SkyLightComponent HDR or JSON fallback. */
		void ResolveSkyLightForFrame(std::optional<std::wstring> componentOverrideFullPath);
		/** After scene swap or scene-texture recycle: next PreProcess::Draw may re-capture IBL (same HDR path is otherwise left as “already init”). */
		void InvalidateSkyLightCapturedEnvironment();

	private:
		PreProcessorPrivate* d_ptr = nullptr;
	};
}
