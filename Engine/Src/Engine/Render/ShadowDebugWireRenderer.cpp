#include "Render/ShadowDebugWireRenderer.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHICommandContext.h"
#include "RHI/RHIViewPort.h"
#include "RHI/RHIRenderPass.h"
#include "Render/RDGUtils.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/RHIShaderDefine.h"
#include "core/system.h"
#include "math/aabb3.h"
#include "math/matrix4x4.h"
#include "math/vector4.h"
#include "Scene/Actor.h"
#include "Scene/SceneMeshComponent.h"
#include "Scene/World.h"
#include "Scene/WorldSceneDebugDraw.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

using namespace RenderCore;

namespace Engine
{
	namespace
	{
		struct CBShadowDebugWire
		{
			math::Matrix4x4 WorldToClip{};
		};

		struct FShadowDebugWireVertex
		{
			math::Vector3 Pos{};
			math::Vector4 Color{ 1.f, 1.f, 1.f, 1.f };
		};

		static constexpr int32_t kMaxWireVertices = 4096;
		static void AppendLine(const math::Vector3& a, const math::Vector3& b, const math::Vector4& rgba, std::vector<FShadowDebugWireVertex>& Out)
		{
			FShadowDebugWireVertex va{}, vb{};
			va.Pos = a;
			va.Color = rgba;
			vb.Pos = b;
			vb.Color = rgba;
			Out.push_back(va);
			Out.push_back(vb);
		}

		static math::Vector3 AnyPerpendicularUnit(const math::Vector3& unitDir)
		{
			math::Vector3 a = std::fabs(unitDir.y) < 0.9f ? math::Vector3(0.f, 1.f, 0.f) : math::Vector3(1.f, 0.f, 0.f);
			math::Vector3 p = math::Vector3::Cross(a, unitDir);
			if (p.GetSqrLength() < 1e-10f)
				p = math::Vector3::Cross(math::Vector3(0.f, 0.f, 1.f), unitDir);
			if (p.GetSqrLength() < 1e-10f)
				return math::Vector3(1.f, 0.f, 0.f);
			return p.Normalize();
		}

		static void AppendArrow(const math::Vector3& origin, const math::Vector3& dirTowardSource, float length, const math::Vector4& rgba,
								std::vector<FShadowDebugWireVertex>& Out)
		{
			math::Vector3 d = dirTowardSource;
			if (d.GetSqrLength() < 1e-10f)
				d = math::Vector3(0.f, 1.f, 0.f);
			else
				d = d.Normalize();
			const float L = (std::max)(0.05f, length);
			const math::Vector3 tip = origin + d * L;
			AppendLine(origin, tip, rgba, Out);

			// small arrow head
			const float head = L * 0.18f;
			const math::Vector3 p = AnyPerpendicularUnit(d);
			const math::Vector3 q = math::Vector3::Cross(d, p).Normalize();
			AppendLine(tip, tip - d * head + p * head * 0.6f, rgba, Out);
			AppendLine(tip, tip - d * head - p * head * 0.6f, rgba, Out);
			AppendLine(tip, tip - d * head + q * head * 0.6f, rgba, Out);
			AppendLine(tip, tip - d * head - q * head * 0.6f, rgba, Out);
		}

		static void AppendWireSphere(const math::Vector3& center, float radius, const math::Vector4& rgba, std::vector<FShadowDebugWireVertex>& Out)
		{
			if (radius <= 1e-4f)
				return;
			const int seg = 24;
			const float r = radius;
			for (int axis = 0; axis < 3; ++axis)
			{
				math::Vector3 prev{};
				for (int i = 0; i <= seg; ++i)
				{
					const float t = (float)i / (float)seg * 2.f * 3.14159265f;
					const float c = std::cos(t);
					const float s = std::sin(t);
					math::Vector3 p{};
					if (axis == 0) p = math::Vector3(0.f, c * r, s * r);
					if (axis == 1) p = math::Vector3(c * r, 0.f, s * r);
					if (axis == 2) p = math::Vector3(c * r, s * r, 0.f);
					p = center + p;
					if (i > 0)
						AppendLine(prev, p, rgba, Out);
					prev = p;
				}
			}
		}

		static void AppendWireCone(const math::Vector3& apex, const math::Vector3& axis, float range, float outerConeCos, const math::Vector4& rgba,
								  std::vector<FShadowDebugWireVertex>& Out)
		{
			float r = range;
			if (!(r > 0.f))
				r = 10.f;
			r = (std::min)(r, 2000.f);
			math::Vector3 a = axis;
			if (a.GetSqrLength() < 1e-10f)
				a = math::Vector3(0.f, 0.f, 1.f);
			else
				a = a.Normalize();

			const float c = (std::max)(-1.f, (std::min)(1.f, outerConeCos));
			const float half = std::acos(c);
			const float baseRadius = std::tan(half) * r;
			const math::Vector3 baseCenter = apex + a * r;
			const math::Vector3 p = AnyPerpendicularUnit(a);
			const math::Vector3 q = math::Vector3::Cross(a, p).Normalize();

			const int seg = 24;
			math::Vector3 first{};
			math::Vector3 prev{};
			for (int i = 0; i <= seg; ++i)
			{
				const float t = (float)i / (float)seg * 2.f * 3.14159265f;
				const float ct = std::cos(t);
				const float st = std::sin(t);
				math::Vector3 onCircle = baseCenter + p * (ct * baseRadius) + q * (st * baseRadius);
				if (i == 0)
					first = onCircle;
				else
					AppendLine(prev, onCircle, rgba, Out);
				prev = onCircle;
			}
			AppendLine(prev, first, rgba, Out);

			// spokes
			for (int i = 0; i < 4; ++i)
			{
				const float t = (float)i / 4.f * 2.f * 3.14159265f;
				const float ct = std::cos(t);
				const float st = std::sin(t);
				math::Vector3 onCircle = baseCenter + p * (ct * baseRadius) + q * (st * baseRadius);
				AppendLine(apex, onCircle, rgba, Out);
			}
		}

		static void AppendWireOrientedBox(const math::Vector3 cornersWorld[8], const math::Vector4& rgba, std::vector<FShadowDebugWireVertex>& Out)
		{
			static const int kEdges[12][2] = {
				{0, 1}, {1, 2}, {2, 3}, {3, 0},
				{4, 5}, {5, 6}, {6, 7}, {7, 4},
				{0, 4}, {1, 5}, {2, 6}, {3, 7},
			};
			for (const auto& e : kEdges)
				AppendLine(cornersWorld[e[0]], cornersWorld[e[1]], rgba, Out);
		}

		static void GatherSceneMeshBoundsIntoSubmit(FShadowDebugWireSubmit& Submit, World& W)
		{
			Submit.NumMeshBounds = 0;
			if (!W.GetSceneDebugDraw().GetShowSceneMeshBoundsDebug())
				return;
			const std::vector<std::shared_ptr<Actor>> actors = W.GetAllActorsCopy();
			for (const auto& act : actors)
			{
				if (!act)
					continue;
				if (Submit.NumMeshBounds >= FShadowDebugWireSubmit::kMaxMeshBoundsBoxes)
					break;
				const auto sm = act->GetComponent<SceneMeshComponent>();
				if (!sm)
					continue;
				const math::AABB3 localBox = sm->GetModelBox();
				if ((localBox.GetMaxPoint() - localBox.GetMinPoint()).GetSqrLength() < 1e-16f)
					continue;
				math::Vector3 corners[8];
				localBox.GetPoint(corners);
				const math::Matrix4x4 M = act->GetWorldTransform();
				FShadowDebugWireSubmit::FMeshBoundsWire box{};
				for (int ci = 0; ci < 8; ++ci)
					box.CornersWorld[ci] = M.TransformPosition(corners[ci]);
				Submit.MeshBounds[Submit.NumMeshBounds++] = box;
			}
		}

		static void GatherShadowCasterMeshBoundsIntoSubmit(FShadowDebugWireSubmit& Submit, World& W)
		{
			Submit.NumShadowCasterMeshBounds = 0;
			if (!W.GetSceneDebugDraw().GetShowShadowCasterMeshBoundsDebug())
				return;
			const std::vector<std::shared_ptr<Actor>> actors = W.GetAllActorsCopy();
			for (const auto& act : actors)
			{
				if (!act)
					continue;
				if (Submit.NumShadowCasterMeshBounds >= FShadowDebugWireSubmit::kMaxMeshBoundsBoxes)
					break;
				const auto sm = act->GetComponent<SceneMeshComponent>();
				if (!sm || !sm->IsProjectShadow())
					continue;
				const math::AABB3 worldBox = sm->GetShadowFrustumWorldBounds();
				if ((worldBox.GetMaxPoint() - worldBox.GetMinPoint()).GetSqrLength() < 1e-16f)
					continue;
				math::Vector3 corners[8];
				worldBox.GetPoint(corners);
				FShadowDebugWireSubmit::FMeshBoundsWire box{};
				for (int ci = 0; ci < 8; ++ci)
					box.CornersWorld[ci] = corners[ci];
				box.Color = math::Vector4(0.95f, 0.38f, 0.08f, 1.f);
				Submit.ShadowCasterMeshBounds[Submit.NumShadowCasterMeshBounds++] = box;
			}
		}

		static void GatherDirectionalCascadeSubjectBoundsIntoSubmit(FShadowDebugWireSubmit& Submit, World& W)
		{
			Submit.NumCascadeSubjectBoxes = 0;
			if (!W.GetSceneDebugDraw().GetShowDirectionalCSMCascadeSubjectBoundsDebug())
				return;
			int n = 0;
			math::AABB3 boxes[3]{};
			W.GetSceneDebugDraw().GetDirectionalCSMCascadeSubjectDebugCopy(n, boxes);
			static const math::Vector4 kCascadeColors[3] = {
				math::Vector4(0.15f, 0.95f, 0.25f, 1.f),
				math::Vector4(0.25f, 0.65f, 1.f, 1.f),
				math::Vector4(1.f, 0.35f, 0.9f, 1.f),
			};
			n = (std::min)(n, FShadowDebugWireSubmit::kMaxCascadeSubjectBoxes);
			for (int i = 0; i < n; ++i)
			{
				if ((boxes[i].GetMaxPoint() - boxes[i].GetMinPoint()).GetSqrLength() < 1e-20f)
					continue;
				math::Vector3 corners[8];
				boxes[i].GetPoint(corners);
				FShadowDebugWireSubmit::FMeshBoundsWire box{};
				for (int ci = 0; ci < 8; ++ci)
					box.CornersWorld[ci] = corners[ci];
				box.Color = kCascadeColors[i];
				Submit.CascadeSubjectBoxes[Submit.NumCascadeSubjectBoxes++] = box;
			}
		}
	} // namespace

	using CBShadowDebugWireWrap = RenderCore::TUniformBufferBinding<CBShadowDebugWire, 0u>;

	struct FShadowDebugWireRendererPrivate
	{
		explicit FShadowDebugWireRendererPrivate(DynamicRHI* InRhi)
			: GET_SHADER_STRUCT_MEMBER(CBShadowDebugWire)(InRhi)
		{
		}
		DECLARE_SHADER_STRUCT_MEMBER(CBShadowDebugWire);
	};

	FShadowDebugWireRenderer::FShadowDebugWireRenderer(DynamicRHI* InRHI)
		: RHI(InRHI)
		, d(std::make_unique<FShadowDebugWireRendererPrivate>(InRHI))
	{
	}

	FShadowDebugWireRenderer::~FShadowDebugWireRenderer() = default;

	void FShadowDebugWireRenderer::InitResource()
	{
		if (!RHI)
			return;
		const std::wstring ShaderPath = core::process_directory().wstring() + L"/ShaderLibDX/ShadowDebugWire.hlsl";

		RHIVertexDeclare Decl;
		Decl.AppendDeclareInput(VertexDeclareInput(0, EVertexElementType::VET_Float3, false));
		Decl.AppendDeclareInput(VertexDeclareInput(1, EVertexElementType::VET_Float4, false));

		VertexShader = RHI->RHICreateVertexShader(ShaderPath, "VS", Decl, {});
		PixelShader = RHI->RHICreatePixelShader(ShaderPath, "PS", {});

		std::vector<FShadowDebugWireVertex> zeros(static_cast<size_t>(kMaxWireVertices));
		const EBufferUsageFlags vbUsage = static_cast<EBufferUsageFlags>(BUF_Dynamic | BUF_VertexBuffer);
		VertexBuffer = RHI->RHICreateVertexBuffer(zeros.data(), vbUsage, sizeof(FShadowDebugWireVertex), kMaxWireVertices);
	}

	void FShadowDebugWireRenderer::Render(RHICommandContext& Ctx, RHIViewPort& ViewPort, FShadowDebugWireSubmit Submit, World* WorldForDebugWire)
	{
		if (!VertexShader || !PixelShader || !VertexBuffer || !d)
			return;

		if (WorldForDebugWire)
		{
			GatherSceneMeshBoundsIntoSubmit(Submit, *WorldForDebugWire);
			GatherShadowCasterMeshBoundsIntoSubmit(Submit, *WorldForDebugWire);
			GatherDirectionalCascadeSubjectBoundsIntoSubmit(Submit, *WorldForDebugWire);
		}

		std::vector<FShadowDebugWireVertex> verts;
		verts.reserve(128);
		for (int i = 0; i < Submit.NumDir; ++i)
			AppendArrow(Submit.Dir[i].Origin, Submit.Dir[i].DirectionTowardSource, Submit.Dir[i].Length, Submit.Dir[i].Color, verts);
		for (int i = 0; i < Submit.NumPoint; ++i)
			AppendWireSphere(Submit.Point[i].Center, Submit.Point[i].Radius, Submit.Point[i].Color, verts);
		for (int i = 0; i < Submit.NumSpot; ++i)
			AppendWireCone(Submit.Spot[i].Apex, Submit.Spot[i].ConeAxis, Submit.Spot[i].Range, Submit.Spot[i].OuterConeCos, Submit.Spot[i].Color, verts);
		for (int i = 0; i < Submit.NumMeshBounds; ++i)
			AppendWireOrientedBox(Submit.MeshBounds[i].CornersWorld, Submit.MeshBounds[i].Color, verts);
		for (int i = 0; i < Submit.NumShadowCasterMeshBounds; ++i)
			AppendWireOrientedBox(Submit.ShadowCasterMeshBounds[i].CornersWorld, Submit.ShadowCasterMeshBounds[i].Color, verts);
		for (int i = 0; i < Submit.NumCascadeSubjectBoxes; ++i)
			AppendWireOrientedBox(Submit.CascadeSubjectBoxes[i].CornersWorld, Submit.CascadeSubjectBoxes[i].Color, verts);

		if (verts.empty())
			return;
		if (static_cast<int32_t>(verts.size()) > kMaxWireVertices)
			verts.resize(static_cast<size_t>(kMaxWireVertices));

		RHI->RHIUpdateVertexBuffer(VertexBuffer, verts.data(), static_cast<int32_t>(verts.size()), sizeof(FShadowDebugWireVertex));

		RHICommandMark Mark(Ctx, "ShadowDebugWire");

		std::shared_ptr<RHITexture2D> BackBuf = ViewPort.GetBackBuffer();
		if (!BackBuf)
			return;

		FRHIRenderPassDesc Om = FRHIRenderPassDesc::SingleColorNoDepth(BackBuf);
		Om.DebugName = "ShadowDebugWire";
		{
			FRDGPassDescriptor B{};
			B.Outputs.push_back({ "BackBuf", [BackBuf]() { return BackBuf; }, true, FRDGResourceAccess::RTV });
			FRDGUtils::AppendPassTextureBarriers(B, Om.DeclaredTextureBarriers);
		}
		FRHIRenderPassScope WirePass(Ctx, std::move(Om));

		const auto sz = ViewPort.GetSize();
		Ctx.SetViewPort(0, 0, sz.x, sz.y);

		GraphicsPipelineStateInitializer Init;
		Init.VertexShader = VertexShader;
		Init.PixelShader = PixelShader;
		Init.PrimitiveType = PT_LineList;
		Init.BlendState = RHICachedStates::BlendTraditional;
		Init.DepthStencilState = RHICachedStates::DepthStateDisable;
		Init.RasterizerState = RHICachedStates::RasterizerStateCullNone;
		Ctx.RHISetGraphicsPipelineState(Init);

		d->GET_UNIFORMDATA(CBShadowDebugWire) = CBShadowDebugWire{};
		d->GET_UNIFORMDATA(CBShadowDebugWire).WorldToClip = Submit.OverlayWorldToClip;
		RHI_UpdateAndBindUniformBuffer(Ctx, d->GET_SHADER_STRUCT_MEMBER(CBShadowDebugWire), SF_Vertex);

		Ctx.DrawPrimitive(VertexBuffer, static_cast<uint32_t>(verts.size()), 0u);
	}
}
