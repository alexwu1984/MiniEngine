#include "Engine/Render/FurMaterialRender.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHICachedStates.h"
#include "RHI/RHIPipeLineState.h"
#include "Engine.h"
#include "Render/MaterialPreFrame.h"
#include "Material/FurMaterial.h"
#include "Render/SceneTextures.h"
#include "GltfModel/GltfMeshBuffer.h"
#include "Material/MaterialBase.h"
#include "Thread/RenderThread.h"
#include "core/system.h"
#include "core/logger.h"
#include "Render/SceneRendering/DeferredLightingPass.h"
#include "Render/WorldSceneRender.h"
#include "Render/SceneRendering/SceneViewData.h"

namespace Engine
{
	using namespace RenderCore;

	struct FurMaterialRenderPrivate
	{
		FurMaterialRenderPrivate()
			:GET_SHADER_STRUCT_MEMBER(CBPerFur)(GEngine->GetRHI().get())
		{
		}

		DECLARE_SHADER_STRUCT_MEMBER(CBPerFur);
		FurConfig Config;
		std::shared_ptr<RenderCore::RHITexture2D> NoiseTex;
		std::shared_ptr<RenderCore::RHIVertexShader> InnerBaseVertexShader;
		std::shared_ptr<RenderCore::RHIPixelShader> InnerBasePixelShader;
	};

	FurMaterialRender::FurMaterialRender(std::shared_ptr<GltfMeshBuffer> MeshBuffer, std::shared_ptr< MaterialBase> MeshMaterial,
										 const FurConfig& InConifg,
										 std::shared_ptr<RenderCore::RHITexture2D> NoiseTex)
		:PBRMaterialRender(MeshBuffer, MeshMaterial)
		,d_ptr(new FurMaterialRenderPrivate())
	{
		C_P(FurMaterialRender);
		d->Config = InConifg;
		d->NoiseTex = NoiseTex;
	}

	FurMaterialRender::~FurMaterialRender()
	{
		delete d_ptr;
	}

	void FurMaterialRender::InitRenderResource()
	{
		PBRMaterialRender::InitRenderResource();
		const std::wstring ShaderRoot = core::process_directory().wstring() + L"/ShaderLibDX/";
		ENQUEUE_UNIQUE_RENDER_COMMAND([this, ShaderRoot](RenderCore::DynamicRHI* RHI)
		{
			C_P(FurMaterialRender);
			if (!RHI || !GetPBRMeshBuffer())
				return;
			const uint32_t VtxFeat = GetPBRMeshBuffer()->GetDeclaredVertexFeatures();
			// Inner base: PBRMaterial.hlsl MainVS/MainPS (same macros as PBRMaterialRender::InitShader).

			RHIVertexDeclare VertexDeclareRHI;
			VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(0, EVertexElementType::VET_Float3, false));
			VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(1, EVertexElementType::VET_Float3, false));
			VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(2, EVertexElementType::VET_Float2, false));
			int32_t DeclIndex = 2;
			VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(++DeclIndex, EVertexElementType::VET_Float4, false));
			if (VtxFeat & MeshBufferVertexFeatures::Skinning)
			{
				VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(++DeclIndex, EVertexElementType::VET_Float4, false));
				VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(++DeclIndex, EVertexElementType::VET_Float4, false));
			}

			std::vector<RHIShaderMacro> InnerMacros;
			if (VtxFeat & MeshBufferVertexFeatures::Skinning)
				InnerMacros.push_back({ "ID_SKINNING_MATRICES","2" });
			if (GetPBRMeshMaterial() && GetPBRMeshMaterial()->WantsRHIBindless() && RHI->GetRHIAPIType() == RHIAPIType::E_D3D12)
				InnerMacros.push_back({ "RHI_BINDLESS", "1" });
			const std::wstring PbrPath = ShaderRoot + L"PBRMaterial.hlsl";
			d->InnerBaseVertexShader = RHI->RHICreateVertexShader(PbrPath, std::string("MainVS"), VertexDeclareRHI, InnerMacros);
			d->InnerBasePixelShader = RHI->RHICreatePixelShader(PbrPath, std::string("MainPS"), InnerMacros);
		});
	}

	void FurMaterialRender::SetPipeLineState(RHICommandContext& RHIContext, std::shared_ptr<FSceneTextures> SceneTextures)
	{
		PBRMaterialRender::SetPipeLineState(RHIContext, SceneTextures);
		if (!SceneTextures)
			return;

		std::vector<std::shared_ptr<RHITexture2D>> Targets = {
			SceneTextures->GetSceneColor(),
			SceneTextures->GetMotionVector(),
			SceneTextures->GetNormalBuffer(),
			SceneTextures->GetEmissiveBuffer(),
			SceneTextures->GetMetallicRoughnessBuffer(),
			SceneTextures->GetMaterialAuxBuffer(),
		};
		RHIContext.SetRenderTarget(Targets, SceneTextures->GetDepth());
	}

	std::wstring FurMaterialRender::GetShaderFileName() const
	{
		return L"FurMaterial.hlsl";
	}

	std::wstring FurMaterialRender::GetVertexShaderFileNameSuffix() const
	{
		return L"FurPass-VS.hlsl";
	}

	void FurMaterialRender::DrawDeferredInnerBase(RHICommandContext& RHIContext, const MaterialRenderParam& RenderParam)
	{
		C_P(FurMaterialRender);
		RenderCore::RHICommandMark Mark(RHIContext, "FurInnerBase");
		if (!RenderParam.SceneTextures || !d->InnerBasePixelShader || !d->InnerBaseVertexShader)
			return;
		StoreRenderParam(RenderParam);
		PBRMaterialRender::SetPipeLineState(RHIContext, RenderParam.SceneTextures);
		GraphicsPipelineStateInitializer Init;
		Init.VertexShader = d->InnerBaseVertexShader;
		Init.PixelShader = d->InnerBasePixelShader;
		Init.BlendState = RHICachedStates::BlendOnAlphaOff;
		Init.DepthStencilState = RHICachedStates::DepthStateEnable;
		Init.RasterizerState = RHICachedStates::RasterizerStateCullBack;
		RHIContext.RHISetGraphicsPipelineState(Init);
		RefreshIBLMipAndRebindPerFrame(RHIContext, RenderParam);
		BindDeferredBaseMaterialTextures(RHIContext);

		auto& FurConfig = d->Config;
		d->GET_UNIFORMDATA(CBPerFur).Gravity = FurConfig.Gravity;
		d->GET_UNIFORMDATA(CBPerFur).FurColor = FurConfig.FurColor;
		d->GET_UNIFORMDATA(CBPerFur).FurLength = 0.f;
		d->GET_UNIFORMDATA(CBPerFur).FurOffset = 0.f;
		d->GET_UNIFORMDATA(CBPerFur).UVScale = FurConfig.UVScale;
		d->GET_UNIFORMDATA(CBPerFur).FurAmbientStrength = FurConfig.FurAmbientStrength;
		d->GET_UNIFORMDATA(CBPerFur).FurLevel = 0.f;
		d->GET_UNIFORMDATA(CBPerFur).FurLightExposure = FurConfig.FurLightExposure;
		d->GET_UNIFORMDATA(CBPerFur).DrawSolid = 0;
		RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, d->GET_SHADER_STRUCT_MEMBER(CBPerFur));

		RHIContext.RHISetShaderSampler(RenderCore::SF_Vertex, 0, RenderCore::RHICachedStates::WarpLinerSampler);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::WarpLinerSampler);

		DrawPrimitive(RHIContext);
	}

	void FurMaterialRender::DrawForwardFur(RHICommandContext& RHIContext, const MaterialRenderParam& RenderParam, DeferredLightingPass* FurSharedBind,
										   FWorldSceneRender* WorldSceneRender, const std::shared_ptr<const FSceneViewData>& ViewData)
	{
		C_P(FurMaterialRender);
		RenderCore::RHICommandMark Mark(RHIContext, "FurForward");
		if (!RenderParam.SceneTextures || !GetPBRVertexShader())
			return;
		if (!GetPBRPixelShader())
		{
			static bool s_LoggedMissingFwdPs = false;
			if (!s_LoggedMissingFwdPs)
			{
				s_LoggedMissingFwdPs = true;
				core::LOG(core::log_err, L"FurMaterialRender: MainPS (pixel shader) is null; forward fur pass is skipped.");
			}
			return;
		}

		StoreRenderParam(RenderParam);
		std::vector<std::shared_ptr<RHITexture2D>> Rt = { RenderParam.SceneTextures->GetSceneColor() };
		RHIContext.SetRenderTarget(Rt, RenderParam.SceneTextures->GetDepth());

		GraphicsPipelineStateInitializer Init;
		Init.VertexShader = GetPBRVertexShader();
		Init.PixelShader = GetPBRPixelShader();
		Init.BlendState = RHICachedStates::BlendTraditional;
		// Match deferred shell path: depth write on so successive shells test against the previous layer, not only the
		// inner base. LessEqual + no-write leaves a single reference depth; extruded shells often end up "behind" that
		// test and clip entirely (no visible fur).
		Init.DepthStencilState = RHICachedStates::DepthStateEnable;
		// Shell extrusion often flips triangle winding relative to the base mesh; CullBack can drop every shell.
		Init.RasterizerState = RHICachedStates::RasterizerStateCullNone;
		RHIContext.RHISetGraphicsPipelineState(Init);

		// D3D12: changing pixel shader clears staged SRV table; bind IBL + shadow (t5–t10) after PSO.
		if (FurSharedBind && WorldSceneRender && ViewData)
			FurSharedBind->BindFurForwardSharedSRVs(RHIContext, RenderParam.SceneTextures, WorldSceneRender, ViewData);

		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RHICachedStates::WarpLinerSampler);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 1, RHICachedStates::ShadowSampler);
		BindDrawUniformBuffers(RHIContext);
		RefreshIBLMipAndRebindPerFrame(RHIContext, RenderParam);

		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, GetPBRMeshMaterial()->GetBaseColorTexture());
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 1, d->NoiseTex);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Vertex, 0, RHICachedStates::WarpLinerSampler);

		auto& FurConfig = d->Config;
		d->GET_UNIFORMDATA(CBPerFur).Gravity = FurConfig.Gravity;
		d->GET_UNIFORMDATA(CBPerFur).FurColor = FurConfig.FurColor;
		d->GET_UNIFORMDATA(CBPerFur).FurLength = FurConfig.FurLength;
		d->GET_UNIFORMDATA(CBPerFur).UVScale = FurConfig.UVScale;
		d->GET_UNIFORMDATA(CBPerFur).FurAmbientStrength = FurConfig.FurAmbientStrength;
		d->GET_UNIFORMDATA(CBPerFur).FurLightExposure = FurConfig.FurLightExposure;
		d->GET_UNIFORMDATA(CBPerFur).DrawSolid = 0;
		d->GET_UNIFORMDATA(CBPerFur).FurLevel = static_cast<float>(FurConfig.FurLevel);

		const int32_t FurLevelCount = (std::max)(1, static_cast<int32_t>(FurConfig.FurLevel));
		// One draw, FurLevel instances: FurVertexFactory derives shell depth from SV_InstanceID when FurLevel >= 1.
		d->GET_UNIFORMDATA(CBPerFur).FurOffset = 0.f;
		RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, d->GET_SHADER_STRUCT_MEMBER(CBPerFur));
		if (GetPBRMeshBuffer())
			RHIContext.DrawPrimitiveInstanced(GetPBRMeshBuffer()->GetVerticesBuffer(), GetPBRMeshBuffer()->GetIndexBuffer(), static_cast<uint32_t>(FurLevelCount), 0u);
	}

	void FurMaterialRender::PreDraw(RHICommandContext& RHIContext, const MaterialRenderParam& RenderParam)
	{
		DrawDeferredInnerBase(RHIContext, RenderParam);
	}

	void FurMaterialRender::DrawMesh(RHICommandContext& RHIContext)
	{
		(void)RHIContext;
	}

	void FurMaterialRender::PreDrawMesh(RHICommandContext& RHIContext)
	{
		(void)RHIContext;
	}

	bool FurMaterialRender::IsNeedPreDraw() const
	{
		return true;
	}

}
