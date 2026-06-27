#include "Engine/Render/PBRMaterialRender.h"
#include "GltfModel/GltfSceneVertexDeclare.h"
#include <algorithm>
#include "Engine.h"
#include "Material/MaterialBase.h"
#include "GltfModel/GltfMeshBuffer.h"
#include "Thread/RenderThread.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/RHIRenderTarget.h"
#include "core/system.h"
#include "core/logger.h"
#include "Render/MaterialPreFrame.h"
#include "Engine/Render/SkyLightEnvironment.h"
#include "Engine/Render/SkyLightEnvironment.h"
#include "Engine/Render/WorldSceneRender.h"
#include "Render/SceneTextures.h"
#include "Render/SceneRendering/DeferredLightingPass.h"
#include "Render/SceneRendering/SceneViewData.h"
#include "RHI/RHITextureCube.h"
#include "RHI/RHIRenderPass.h"
#include "Render/RDGUtils.h"

namespace Engine
{
	using namespace RenderCore;

	namespace
	{
		/** Translucent forward PS input does not depend on skinning / base-pass alpha macros; dropping them cuts JIT permutation count. */
		void FilterMacrosTranslucentForwardPS(const std::vector<RHIShaderMacro>& Src, std::vector<RHIShaderMacro>& Out)
		{
			Out.clear();
			Out.reserve(Src.size());
			for (const RHIShaderMacro& M : Src)
			{
				if (M.Name == "ID_SKINNING_MATRICES")
					continue;
				Out.push_back(M);
			}
		}
	}

	struct PBRMaterialRenderPrivate
	{
		PBRMaterialRenderPrivate()
			: GET_SHADER_STRUCT_MEMBER(CBPerSkeleton)(GEngine->GetRHI().get()),
			  GET_SHADER_STRUCT_MEMBER(CBPerFrame)(GEngine->GetRHI().get()),
			  GET_SHADER_STRUCT_MEMBER(CBPerObject)(GEngine->GetRHI().get()),
			  GET_SHADER_STRUCT_MEMBER(CBPerMaterial)(GEngine->GetRHI().get())
		{
		}
		std::shared_ptr<GltfMeshBuffer> MeshBuffer;
		std::shared_ptr<MaterialBase> MeshMaterial;
		std::shared_ptr<RHIVertexShader> VertexShader;
		std::shared_ptr<RHIPixelShader> PixelShader;
		std::shared_ptr<RHIPixelShader> TranslucentForwardPixelShader;
		MaterialRenderParam RenderParam;
		bool bSkeletonMotionHistoryPrimed = false;

		DECLARE_SHADER_STRUCT_MEMBER(CBPerFrame)
		DECLARE_SHADER_STRUCT_MEMBER(CBPerObject)
		DECLARE_SHADER_STRUCT_MEMBER(CBPerMaterial)
		DECLARE_SHADER_STRUCT_MEMBER(CBPerSkeleton)
	};

	PBRMaterialRender::PBRMaterialRender(std::shared_ptr<GltfMeshBuffer> MeshBuffer, std::shared_ptr< MaterialBase> MeshMaterial)
		:d_ptr(new PBRMaterialRenderPrivate())
	{
		
		C_P(PBRMaterialRender);
		d->MeshBuffer = MeshBuffer;
		d->MeshMaterial = MeshMaterial;
	}

	PBRMaterialRender::~PBRMaterialRender()
	{
		delete d_ptr;
	}

	void PBRMaterialRender::InitRenderResource()
	{
		std::wstring ShaderPath = core::process_directory().wstring() + L"/ShaderLibDX/";
		InitShader(ShaderPath);
	}

	void PBRMaterialRender::SetBoneMatrix(const math::Matrix4x4& Mat, int32_t Index)
	{
		C_P(PBRMaterialRender);
		if (Index < 0 || Index >= CBPerSkeleton::kPaletteMatrixCount)
			return;
		auto const& PreviousMatrix = d->GET_UNIFORMDATA(CBPerSkeleton).PerSkeleton_u_ModelMatrix[Index].Current;
		d->GET_UNIFORMDATA(CBPerSkeleton).PerSkeleton_u_ModelMatrix[Index].Previous = PreviousMatrix;
		d->GET_UNIFORMDATA(CBPerSkeleton).PerSkeleton_u_ModelMatrix[Index].Current = Mat;
	}

	void PBRMaterialRender::ResetSkeletonPaletteIdentity()
	{
		C_P(PBRMaterialRender);
		math::Matrix4x4 I;
		I.Identity();
		auto& Data = d->GET_UNIFORMDATA(CBPerSkeleton);
		for (int i = 0; i < CBPerSkeleton::kPaletteMatrixCount; ++i)
		{
			Data.PerSkeleton_u_ModelMatrix[i].Current = I;
			Data.PerSkeleton_u_ModelMatrix[i].Previous = I;
		}
		d->bSkeletonMotionHistoryPrimed = false;
	}

	void PBRMaterialRender::OnSkinnedPaletteUploaded(int32_t NumBones)
	{
		C_P(PBRMaterialRender);
		if (d->bSkeletonMotionHistoryPrimed || NumBones <= 0)
			return;
		auto& Data = d->GET_UNIFORMDATA(CBPerSkeleton);
		const int32_t N = (std::min)(NumBones, static_cast<int32_t>(CBPerSkeleton::kPaletteMatrixCount));
		for (int32_t i = 0; i < N; ++i)
			Data.PerSkeleton_u_ModelMatrix[i].Previous = Data.PerSkeleton_u_ModelMatrix[i].Current;
		d->bSkeletonMotionHistoryPrimed = true;
	}

	std::wstring PBRMaterialRender::GetShaderFileName() const
	{
		return L"PBRMaterial.hlsl";
	}

	void PBRMaterialRender::InitShader(const std::wstring& Path)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND([Path, this](RenderCore::DynamicRHI* RHI) {
			C_P(PBRMaterialRender);
			const std::wstring psPath = Path + GetShaderFileName();
			const std::wstring vsSuffix = GetVertexShaderFileNameSuffix();
			const std::wstring vsPath = vsSuffix.empty() ? psPath : (Path + vsSuffix);

			const uint32_t VtxFeat = d->MeshBuffer->GetDeclaredVertexFeatures();
			RHIVertexDeclare VertexDeclareRHI;
			BuildGltfSceneVertexDeclare(VtxFeat, VertexDeclareRHI);

			std::vector< RHIShaderMacro> ShaderMacros;
			AppendGltfSceneSkinningShaderMacros(VtxFeat, ShaderMacros);

			if (d->MeshMaterial && d->MeshMaterial->WantsRHIBindless() && RHI->GetRHIAPIType() == RHIAPIType::E_D3D12)
				ShaderMacros.push_back({ "RHI_BINDLESS", "1" });

			d->VertexShader = RHI->RHICreateVertexShader(vsPath, "MainVS", VertexDeclareRHI, ShaderMacros);
			d->PixelShader = RHI->RHICreatePixelShader(psPath, "MainPS", ShaderMacros);

			if (this->ShouldCompileTranslucentForwardPixelShader())
			{
				const std::wstring ForwardPath = Path + L"TranslucentPBRForward.hlsl";
				std::vector<RHIShaderMacro> TranslucentMacros;
				FilterMacrosTranslucentForwardPS(ShaderMacros, TranslucentMacros);
				d->TranslucentForwardPixelShader = RHI->RHICreatePixelShader(ForwardPath, std::string("MainPS_TranslucentForward"), TranslucentMacros);
			}
			});
		
	}

	bool PBRMaterialRender::SetPipeLineState(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<FSceneTextures> SceneTextures)
	{
		C_P(PBRMaterialRender);
		if (!d->VertexShader || !d->PixelShader)
		{
			static bool s_LoggedMissingBaseShaders = false;
			if (!s_LoggedMissingBaseShaders)
			{
				s_LoggedMissingBaseShaders = true;
				core::LOG(core::log_err,
						  L"PBRMaterialRender: vertex or pixel shader is null (compile failed or InitShader not run); deferred draws skipped.");
			}
			return false;
		}
		if (!SceneTextures)
			return false;

		GraphicsPipelineStateInitializer Init;
		Init.PixelShader = d->PixelShader;
		Init.VertexShader = d->VertexShader;

		if (d->MeshMaterial->IsTransparent())
		{
			Init.BlendState = RHICachedStates::BlendTraditional;
			// UE-style: textured translucency writes scene depth; constant-alpha translucency depth-tests only.
			if (d->MeshMaterial->WritesTranslucentDepthToSceneBuffer())
				Init.DepthStencilState = RHICachedStates::DepthStateEnable;
			else
				Init.DepthStencilState = RHICachedStates::DepthStateLessEqualNoWrite;
		}
		else
		{
			Init.BlendState = RHICachedStates::BlendOnAlphaOff;
			Init.DepthStencilState = RHICachedStates::DepthStateEnable;
		}
		Init.RasterizerState =
			d->MeshMaterial->IsDoubleSided() ? RHICachedStates::RasterizerStateCullNone : RHICachedStates::RasterizerStateCullBack;

		std::vector <std::shared_ptr<RenderCore::RHITexture2D>> Targets = { SceneTextures->GetSceneColor(),SceneTextures->GetMotionVector(),SceneTextures->GetNormalBuffer(),
					SceneTextures->GetEmissiveBuffer(),SceneTextures->GetMetallicRoughnessBuffer() };
		RHIContext.SetRenderTarget(Targets, SceneTextures->GetDepth());
		RHIContext.RHISetGraphicsPipelineState(Init);

		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RHICachedStates::WarpLinerSampler);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 1, RHICachedStates::ShadowSampler);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 2, RHICachedStates::ShadowCompareSampler);

		BindDrawUniformBuffers(RHIContext);
		return true;
	}

	void PBRMaterialRender::StoreRenderParam(const MaterialRenderParam& p)
	{
		C_P(PBRMaterialRender);
		d->RenderParam = p;
	}

	void PBRMaterialRender::BindDrawUniformBuffers(RenderCore::RHICommandContext& RHIContext)
	{
		C_P(PBRMaterialRender);
		d->GET_UNIFORMDATA(CBPerObject).myPerObject_u_mCurrWorld = d->RenderParam.CurrModelMatrix;
		d->GET_UNIFORMDATA(CBPerObject).myPerObject_u_mPrevWorld = d->RenderParam.PrevModelMatrix;
		RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, d->GET_SHADER_STRUCT_MEMBER(CBPerObject));

		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.CameraPrevViewProj = d->RenderParam.PrevViewProjMatrix;
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.CameraCurrViewProj = d->RenderParam.CurrViewProjMatrix;
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.CameraCurrViewProjInverse = d->RenderParam.CurrViewProjInverseMatrix;
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.CameraWorldToView = d->RenderParam.CameraWorldToView;
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.RotateIBL = d->RenderParam.RotateIBL;
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.CameraPos = d->RenderParam.CameraPos;
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.InvScreenResolution = d->RenderParam.InvScreenResolution;
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.TemporalAAJitter = d->RenderParam.TemporalAAJitter;
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.IBLFactor = d->RenderParam.SkyLightIBLScale;
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.IBLDirShadowCoupling =
			math::Vector4(d->RenderParam.IBLDiffuseDirShadowCoupling, d->RenderParam.IBLSpecularDirShadowCoupling,
							d->RenderParam.IBLDiffuseAoExponentForIBL, 0.f);

		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.LightCount = (int32_t)d->RenderParam.lightInfos.size();
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.PrimaryDirectionalLightIndex = d->RenderParam.PrimaryDirectionalLightIndex;
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.bUnlit = d->RenderParam.bUnlit ? 1 : 0;
		for (int32_t index = 0; index < (int32_t)d->RenderParam.lightInfos.size(); ++index)
		{
			d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.Lights[index] = d->RenderParam.lightInfos[index];
		}
		RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, d->GET_SHADER_STRUCT_MEMBER(CBPerFrame));

		d->GET_UNIFORMDATA(CBPerMaterial).myMaterial.Metallic = d->MeshMaterial->GetMaterialConfig().Metallic;
		d->GET_UNIFORMDATA(CBPerMaterial).myMaterial.AlphaCutoff = d->MeshMaterial->GetMaterialAlphaCutoff();
		d->GET_UNIFORMDATA(CBPerMaterial).myMaterial.TransmissionFactor = d->MeshMaterial->GetTransmissionFactor();
		d->GET_UNIFORMDATA(CBPerMaterial).myMaterial.AttenuationDistance = d->MeshMaterial->GetAttenuationDistance();
		d->GET_UNIFORMDATA(CBPerMaterial).myMaterial.AttenuationColor = d->MeshMaterial->GetAttenuationColor();
		d->GET_UNIFORMDATA(CBPerMaterial).myMaterial.ThicknessFactor = d->MeshMaterial->GetThicknessFactor();
		d->GET_UNIFORMDATA(CBPerMaterial).myMaterial.MaterialIor = d->MeshMaterial->GetMaterialIor();
		d->GET_UNIFORMDATA(CBPerMaterial).myMaterial.MaterialDispersion = d->MeshMaterial->GetMaterialDispersion();
		d->GET_UNIFORMDATA(CBPerMaterial).myMaterial.AlphaMask = d->MeshMaterial->UsesMaterialAlphaMask() ? 1u : 0u;
		uint32_t shaderFlags = 0u;
		if (d->MeshMaterial->IsTransparent())
			shaderFlags |= kMaterialShaderFlag_WriteBaseColorAlphaToGBuffer;
		if (d->MeshMaterial->IsDoubleSided())
			shaderFlags |= kMaterialShaderFlag_DoubleSidedShading;
		if (d->MeshMaterial->UsesTransmissionShading())
			shaderFlags |= kMaterialShaderFlag_Transmission;
		d->GET_UNIFORMDATA(CBPerMaterial).myMaterial.MaterialShaderFlags = shaderFlags;
		RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, d->GET_SHADER_STRUCT_MEMBER(CBPerMaterial));

		if (d->RenderParam.HasSkin)
			RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, d->GET_SHADER_STRUCT_MEMBER(CBPerSkeleton));
	}

	void PBRMaterialRender::RefreshIBLMipAndRebindPerFrame(RenderCore::RHICommandContext& RHIContext, const MaterialRenderParam& RenderParam)
	{
		C_P(PBRMaterialRender);
		if (!RenderParam.skylightEnvironment.expired())
		{
			if (auto IBL = RenderParam.skylightEnvironment.lock())
			{
				if (auto SpecCube = IBL->GetSpecularReflectionCubemap())
					d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.IBLMIpCount =
						static_cast<float>(std::max<uint32_t>(SpecCube->GetNumMips(), 1u));
			}
		}
		else
			d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.IBLMIpCount = 1.f;
		RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, d->GET_SHADER_STRUCT_MEMBER(CBPerFrame));
	}

	void PBRMaterialRender::BindDeferredBaseMaterialTextures(RenderCore::RHICommandContext& RHIContext)
	{
		C_P(PBRMaterialRender);
		FRDGUtils::RHICmdListDeclarePixelSamplingSrvs(RHIContext,
													  {
														  d->MeshMaterial->GetBaseColorTexture(),
														  d->MeshMaterial->GetNormalTexture(),
														  d->MeshMaterial->GetMetallicRoughnessTexture(),
														  d->MeshMaterial->GetEmissiveTexture(),
														  d->MeshMaterial->GetOcclusionTexture(),
													  });
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, d->MeshMaterial->GetBaseColorTexture());
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 1, d->MeshMaterial->GetNormalTexture());
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 2, d->MeshMaterial->GetMetallicRoughnessTexture());
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 3, d->MeshMaterial->GetEmissiveTexture());
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 4, d->MeshMaterial->GetOcclusionTexture());
	}

	void PBRMaterialRender::DrawTranslucentForwardLit(RenderCore::RHICommandContext& RHIContext, const MaterialRenderParam& RenderParam, uintptr_t* InOutSharedSrvsBoundForPsKey, DeferredLightingPass* DeferredLighting,
													  FWorldSceneRender* WorldSceneRender, const std::shared_ptr<const FSceneViewData>& ViewData)
	{
		C_P(PBRMaterialRender);
		if (!d->MeshMaterial || !d->MeshMaterial->IsTransparent() || !RenderParam.SceneTextures || !GetPBRVertexShader())
			return;
		if (!d->TranslucentForwardPixelShader)
		{
			static bool s_LoggedMissingFwdTranslucentPs = false;
			if (!s_LoggedMissingFwdTranslucentPs)
			{
				s_LoggedMissingFwdTranslucentPs = true;
				core::LOG(core::log_err, L"PBRMaterialRender: MainPS_TranslucentForward is null; translucent forward pass skipped.");
			}
			return;
		}

		RenderCore::RHICommandMark Mark(RHIContext, "TranslucentPBRForward");
		StoreRenderParam(RenderParam);

		const bool bTransmission = d->MeshMaterial->UsesTransmissionShading();
		const std::shared_ptr<RHITexture2D> TransmissionBg =
			bTransmission ? RenderParam.SceneTextures->GetSceneColorWithSSR() : nullptr;

		GraphicsPipelineStateInitializer Init;
		Init.VertexShader = GetPBRVertexShader();
		Init.PixelShader = d->TranslucentForwardPixelShader;
		Init.BlendState = bTransmission ? RHICachedStates::BlendDisable : RHICachedStates::BlendForwardColorAndVelocityMRT;
		Init.DepthStencilState = RHICachedStates::DepthStateLessEqualNoWrite;
		Init.RasterizerState =
			d->MeshMaterial->IsDoubleSided() ? RHICachedStates::RasterizerStateCullNone : RHICachedStates::RasterizerStateCullBack;
		RHIContext.RHISetGraphicsPipelineState(Init);

		if (InOutSharedSrvsBoundForPsKey && DeferredLighting && WorldSceneRender && ViewData && d->TranslucentForwardPixelShader)
		{
			const uintptr_t psKey = reinterpret_cast<uintptr_t>(d->TranslucentForwardPixelShader.get());
			if (*InOutSharedSrvsBoundForPsKey != psKey)
			{
				DeferredLighting->BindFurForwardSharedSRVs(RHIContext, RenderParam.SceneTextures, WorldSceneRender, ViewData);
				*InOutSharedSrvsBoundForPsKey = psKey;
			}
		}

		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RHICachedStates::WarpLinerSampler);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 1, RHICachedStates::ShadowSampler);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 2, RHICachedStates::ShadowCompareSampler);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Vertex, 0, RHICachedStates::WarpLinerSampler);

		BindDrawUniformBuffers(RHIContext);
		RefreshIBLMipAndRebindPerFrame(RHIContext, RenderParam);
		BindDeferredBaseMaterialTextures(RHIContext);
		if (TransmissionBg)
			RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 9, TransmissionBg);

		DrawPrimitive(RHIContext);
	}

	void PBRMaterialRender::Draw(RenderCore::RHICommandContext& RHIContext, const MaterialRenderParam& RenderParam)
	{
		C_P(PBRMaterialRender);
		RenderCore::RHICommandMark Mark(RHIContext, "PBRPass");
		d->RenderParam = RenderParam;
		if (!SetPipeLineState(RHIContext, RenderParam.SceneTextures))
			return;

		RefreshIBLMipAndRebindPerFrame(RHIContext, RenderParam);
		BindDeferredBaseMaterialTextures(RHIContext);

		DrawMesh(RHIContext);
	}

	void PBRMaterialRender::BeginDeferredOpaqueDrawBatch(RenderCore::RHICommandContext& RHIContext, const MaterialRenderParam& RenderParam)
	{
		C_P(PBRMaterialRender);
		RenderCore::RHICommandMark Mark(RHIContext, "PBRPassBatchBegin");
		d->RenderParam = RenderParam;
		if (!SetPipeLineState(RHIContext, RenderParam.SceneTextures))
			return;
		RefreshIBLMipAndRebindPerFrame(RHIContext, RenderParam);
		BindDeferredBaseMaterialTextures(RHIContext);
		DrawMesh(RHIContext);
	}

	void PBRMaterialRender::DrawDeferredOpaqueBatchInstance(RenderCore::RHICommandContext& RHIContext, const MaterialRenderParam& RenderParam)
	{
		C_P(PBRMaterialRender);
		if (RenderParam.HasSkin)
			return;

		d->RenderParam = RenderParam;
		d->GET_UNIFORMDATA(CBPerObject).myPerObject_u_mCurrWorld = RenderParam.CurrModelMatrix;
		d->GET_UNIFORMDATA(CBPerObject).myPerObject_u_mPrevWorld = RenderParam.PrevModelMatrix;
		RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, d->GET_SHADER_STRUCT_MEMBER(CBPerObject));

		DrawMesh(RHIContext);
	}

	void PBRMaterialRender::PreDraw(RenderCore::RHICommandContext& RHIContext, const MaterialRenderParam& RenderParam)
	{
		C_P(PBRMaterialRender);
		if (!IsNeedPreDraw())
		{
			return;
		}
		RenderCore::RHICommandMark Mark(RHIContext, "PBRPrePass");
		d->RenderParam = RenderParam;

		if (!SetPipeLineState(RHIContext, RenderParam.SceneTextures))
			return;
		FRDGUtils::RHICmdListDeclarePixelSamplingSrvs(RHIContext, { d->MeshMaterial->GetBaseColorTexture() });
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, d->MeshMaterial->GetBaseColorTexture());
		PreDrawMesh(RHIContext);
	}

	void PBRMaterialRender::DrawPrimitive(RenderCore::RHICommandContext& RHIContext)
	{
		C_P(PBRMaterialRender);
		// FMeshMaterialRenderCache pairs this instance with d->MeshBuffer; swapping in DrawMeshBuffer when only
		// DeclaredVertexFeatures match could draw another submesh's positions with this material's maps.
		if (d->MeshBuffer)
			RHIContext.DrawPrimitive(d->MeshBuffer->GetVerticesBuffer(), d->MeshBuffer->GetIndexBuffer());
	}

	void PBRMaterialRender::DrawMesh(RenderCore::RHICommandContext& RHIContext)
	{
		DrawPrimitive(RHIContext);
	}

	void PBRMaterialRender::PreDrawMesh(RenderCore::RHICommandContext& RHIContext)
	{

	}

	bool PBRMaterialRender::IsNeedPreDraw() const
	{
		return false;
	}

	const MaterialRenderParam& PBRMaterialRender::GetRenderParam() const
	{
		C_P(const PBRMaterialRender);
		return d->RenderParam;
	}

	std::shared_ptr<GltfMeshBuffer> PBRMaterialRender::GetPBRMeshBuffer() const
	{
		C_P(const PBRMaterialRender);
		return d->MeshBuffer;
	}

	std::shared_ptr<RHIVertexShader> PBRMaterialRender::GetPBRVertexShader() const
	{
		C_P(const PBRMaterialRender);
		return d->VertexShader;
	}

	std::shared_ptr<RHIPixelShader> PBRMaterialRender::GetPBRPixelShader() const
	{
		C_P(const PBRMaterialRender);
		return d->PixelShader;
	}

	std::shared_ptr<MaterialBase> PBRMaterialRender::GetPBRMeshMaterial() const
	{
		C_P(const PBRMaterialRender);
		return d->MeshMaterial;
	}

}