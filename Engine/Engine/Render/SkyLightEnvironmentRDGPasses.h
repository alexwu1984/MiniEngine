#pragma once

namespace RenderCore
{
	class RHICommandContext;
}

namespace Engine
{
	class FRDGBuilder;
	class USkyLightComponent;

	/** Registers skylight IBL bake steps on an FRDGBuilder (decoupled from USkyLightComponent surface API). */
	struct FSkyLightEnvironmentRDGPasses
	{
		static void RegisterPasses(USkyLightComponent& Skylight, FRDGBuilder& Graph, RenderCore::RHICommandContext& RHIContext);
	};
} // namespace Engine
