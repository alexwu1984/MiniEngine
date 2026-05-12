#pragma once
#include "math/matrix4x4.h"
#include "math/vector3.h"
#include "math/vector4.h"
#include <cstdint>
#include <memory>

namespace RenderCore
{
	class DynamicRHI;
	class RHICommandContext;
	class RHIViewPort;
	class RHIVertexBuffer;
	class RHIVertexShader;
	class RHIPixelShader;
}

namespace Engine
{
	class World;
	struct FShadowDebugWireRendererPrivate;

	/** Filled on the render thread; consumed by the wire overlay pass. */
	struct FShadowDebugWireSubmit
	{
		/** Main view world→clip without TAA jitter (matches shading matrices; avoids screen-space skew vs ImGui viewport). */
		math::Matrix4x4 OverlayWorldToClip{};

		// UE4-style light shape gizmos (not shadow frustums). Fixed-size arrays: multiple lights can be toggled without allocations.
		static constexpr int kMaxDebugLights = 8;

		struct FDirArrow
		{
			math::Vector3 Origin{};
			math::Vector3 DirectionTowardSource{ 0.f, 1.f, 0.f };
			float Length = 2.5f;
			math::Vector4 Color{ 0.95f, 0.95f, 0.95f, 1.f };
		};
		struct FPointSphere
		{
			math::Vector3 Center{};
			float Radius = 0.f;
			math::Vector4 Color{ 0.35f, 0.75f, 1.f, 1.f };
		};
		struct FSpotCone
		{
			math::Vector3 Apex{};
			math::Vector3 ConeAxis{ 0.f, 0.f, 1.f }; // world emission axis
			float Range = 10.f;
			float OuterConeCos = 0.70710677f; // cos(half-angle)
			math::Vector4 Color{ 1.f, 0.85f, 0.2f, 1.f };
		};

		int NumDir = 0;
		FDirArrow Dir[kMaxDebugLights]{};
		int NumPoint = 0;
		FPointSphere Point[kMaxDebugLights]{};
		int NumSpot = 0;
		FSpotCone Spot[kMaxDebugLights]{};

		/** Oriented mesh local bounds in world (8 corners per box). Filled inside FShadowDebugWireRenderer::Render when enabled. */
		static constexpr int kMaxMeshBoundsBoxes = 48;
		struct FMeshBoundsWire
		{
			math::Vector3 CornersWorld[8]{};
			math::Vector4 Color{ 0.93f, 0.93f, 0.1f, 1.f };
		};
		int NumMeshBounds = 0;
		FMeshBoundsWire MeshBounds[kMaxMeshBoundsBoxes]{};

		/** World-space AABB wire per shadow-casting mesh (GetShadowFrustumWorldBounds). Filled in FShadowDebugWireRenderer::Render when enabled. */
		int NumShadowCasterMeshBounds = 0;
		FMeshBoundsWire ShadowCasterMeshBounds[kMaxMeshBoundsBoxes]{};
	};

	/** GPU line-list overlay for debug gizmos. */
	class FShadowDebugWireRenderer
	{
	public:
		explicit FShadowDebugWireRenderer(RenderCore::DynamicRHI* InRHI);
		~FShadowDebugWireRenderer();

		void InitResource();
		/** @param WorldForDebugWire If non-null, may append mesh-bounds and/or shadow-caster-bounds wires from World flags before draw. */
		void Render(RenderCore::RHICommandContext& Ctx, RenderCore::RHIViewPort& ViewPort, FShadowDebugWireSubmit Submit, World* WorldForDebugWire);

	private:
		RenderCore::DynamicRHI* RHI = nullptr;
		std::shared_ptr<RenderCore::RHIVertexShader> VertexShader;
		std::shared_ptr<RenderCore::RHIPixelShader> PixelShader;
		std::shared_ptr<RenderCore::RHIVertexBuffer> VertexBuffer;
		std::unique_ptr<FShadowDebugWireRendererPrivate> d;
	};
}
