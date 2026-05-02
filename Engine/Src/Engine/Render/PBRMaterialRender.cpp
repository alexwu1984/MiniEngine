#include "Engine/Render/PBRMaterialRender.h"
#include "Engine.h"
#include "Material/MaterialBase.h"
#include "GltfModel/GltfMeshBuffer.h"
#include "Thread/RenderThread.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/RHIRenderTarget.h"
#include "core/system.h"
#include "Render/MaterialPreFrame.h"
#include "Engine/Render/PreProcessor.h"
#include "Engine/Render/SkyLightEnvironment.h"
#include "Engine/Render/WorldSceneRender.h"
#include "Render/SceneTextures.h"
#include "RHI/RHITextureCube.h"
#include <algorithm>

namespace Engine
{
	using namespace RenderCore;

	struct PBRMaterialRenderPrivate
	{
		PBRMaterialRenderPrivate() :GET_SHADER_STRUCT_MEMBER(CBPerSkeleton)(GEngine->GetRHI().get()),
			GET_SHADER_STRUCT_MEMBER(CBPerFrame)(GEngine->GetRHI().get()),
			GET_SHADER_STRUCT_MEMBER(CBPerObject)(GEngine->GetRHI().get())
		{
		}
		std::shared_ptr<GltfMeshBuffer> MeshBuffer;
		std::shared_ptr<MaterialBase> MeshMaterial;
		std::shared_ptr<RHIVertexShader> VertexShader;
		std::shared_ptr<RHIPixelShader> PixelShader;
		MaterialRenderParam RenderParam;
		bool bSkeletonMotionHistoryPrimed = false;

		DECLARE_SHADER_STRUCT_MEMBER(CBPerFrame)
		DECLARE_SHADER_STRUCT_MEMBER(CBPerObject)
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

	void PBRMaterialRender::AddShaderMacro(std::vector< RenderCore::RHIShaderMacro>& ShaderMacros)
	{

	}

	void PBRMaterialRender::InitShader(const std::wstring& Path)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND([Path, this](RenderCore::DynamicRHI* RHI) {
			C_P(PBRMaterialRender);
			std::wstring ShaderPath = Path + GetShaderFileName();

			const uint32_t VtxFeat = d->MeshBuffer->GetDeclaredVertexFeatures();
			RHIVertexDeclare VertexDeclareRHI;
			VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(0, EVertexElementType::VET_Float3, false));
			VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(1, EVertexElementType::VET_Float3, false));
			VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(2, EVertexElementType::VET_Float2, false));

			std::vector< RHIShaderMacro> ShaderMacros;
			if (VtxFeat & MeshBufferVertexFeatures::Skinning)
			{
				ShaderMacros.push_back({ "ID_SKINNING_MATRICES","2" });
			}
			int32_t Index = 2;
			if (VtxFeat & MeshBufferVertexFeatures::Tangent)
			{
				VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(++Index, EVertexElementType::VET_Float4, false));
				ShaderMacros.push_back({ "HAS_TANGENT","1" });
			}

			if (VtxFeat & MeshBufferVertexFeatures::Skinning)
			{
				VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(++Index, EVertexElementType::VET_Float4, false));
				ShaderMacros.push_back({ "HAS_WEIGHTS_0","1" });
			}

			//ShaderMacros.push_back({"MATERIAL_UNLIT","0"});

			if (VtxFeat & MeshBufferVertexFeatures::Skinning)
			{
				VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(++Index, EVertexElementType::VET_Float4, false));
			}

			AddShaderMacro(ShaderMacros);

			if (d->MeshMaterial->IsTransparent())
				ShaderMacros.push_back({ "WRITE_BASECOLOR_ALPHA_TO_GBUFFER", "1" });

			// D3D12: batch first five material SRVs as one ps_5_1 texture array (see RHI_BINDLESS in PBRMaterial.hlsl).
			if (RHI && lstrcmp(RHI->GetName(), TEXT("D3D12")) == 0)
				ShaderMacros.push_back({ "RHI_BINDLESS", "1" });

			d->VertexShader = RHI->RHICreateVertexShader(ShaderPath, "MainVS", VertexDeclareRHI, ShaderMacros);
			d->PixelShader = RHI->RHICreatePixelShader(ShaderPath, "MainPS", ShaderMacros);
			});
		
	}

	void PBRMaterialRender::SetPipeLineState(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<SceneTextures> TargetBuffer)
	{
		C_P(PBRMaterialRender);
		GraphicsPipelineStateInitializer Init;
		Init.PixelShader = d->PixelShader;
		Init.VertexShader = d->VertexShader;

		if (d->MeshMaterial->IsTransparent())
		{
			Init.BlendState = RHICachedStates::BlendTraditional;
			Init.DepthStencilState = RHICachedStates::DepthStateDisable;
		}
		else
		{
			Init.BlendState = RHICachedStates::BlendOnAlphaOff;
			Init.DepthStencilState = RHICachedStates::DepthStateEnable;
		}
		Init.RasterizerState = RHICachedStates::RasterizerStateCullBack;

		std::vector <std::shared_ptr<RenderCore::RHITexture2D>> Targets = { TargetBuffer->GetSceneColor(),TargetBuffer->GetMotionVector(),TargetBuffer->GetNormalBuffer(),
					TargetBuffer->GetEmissiveBuffer(),TargetBuffer->GetMetallicRoughnessBuffer() };
		RHIContext.SetRenderTarget(Targets, TargetBuffer->GetDepth());
		RHIContext.RHISetGraphicsPipelineState(Init);

		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RHICachedStates::WarpLinerSampler);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 1, RHICachedStates::ShadowSampler);

		//to do,set uniform buffer
		d->GET_UNIFORMDATA(CBPerObject).myPerObject_u_mCurrWorld = d->RenderParam.CurrModelMatrix;
		d->GET_UNIFORMDATA(CBPerObject).myPerObject_u_mPrevWorld = d->RenderParam.PrevModelMatrix;
		RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, d->GET_SHADER_STRUCT_MEMBER(CBPerObject));

		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.CameraPrevViewProj = d->RenderParam.PrevViewProjMatrix;
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.CameraCurrViewProj = d->RenderParam.CurrViewProjMatrix;
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.CameraCurrViewProjInverse = d->RenderParam.CurrViewProjInverseMatrix;
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.RotateIBL = d->RenderParam.RotateIBL;
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.CameraPos = d->RenderParam.CameraPos;
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.TemporalAAJitter = d->RenderParam.TemporalAAJitter;
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.IBLFactor = d->RenderParam.SkyLightIBLScale;

		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.LightCount = (int32_t)d->RenderParam.lightInfos.size();
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.bUnlit = d->RenderParam.bUnlit ? 1 : 0;
		for (int32_t index = 0; index < (int32_t)d->RenderParam.lightInfos.size(); ++index)
		{
			d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.Lights[index] = d->RenderParam.lightInfos[index];
		}
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.Material.Metallic = d->MeshMaterial->GetMaterialConfig().Metallic;
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.Material.AlphaCutoff = d->MeshMaterial->GetMaterialAlphaCutoff();
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.Material.AlphaMask = d->MeshMaterial->UsesMaterialAlphaMask() ? 1u : 0u;
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.Material.Padding = 0u;

		RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, d->GET_SHADER_STRUCT_MEMBER(CBPerFrame));

		if (d->RenderParam.HasSkin)
			RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, d->GET_SHADER_STRUCT_MEMBER(CBPerSkeleton));
	}

	void PBRMaterialRender::Draw(RenderCore::RHICommandContext& RHIContext, const MaterialRenderParam& RenderParam)
	{
		C_P(PBRMaterialRender);
		RenderCore::RHICommandMark Mark(RHIContext, "PBRPass");
		d->RenderParam = RenderParam;
		SetPipeLineState(RHIContext, RenderParam.TargetBuffer);

		// Match specular prefilter cubemap mips; default IBLMIpCount==1 made lod/BRDF mip math invalid (black metals, dull scene).
		if (!RenderParam.preProcessor.expired())
		{
			if (auto Pre = RenderParam.preProcessor.lock())
			{
				if (auto SkyLightEnv = Pre->GetSkyLightEnvironment())
				{
					if (auto SpecCube = SkyLightEnv->GetSpecularReflectionCubemap())
						d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.IBLMIpCount = static_cast<float>(std::max<uint32_t>(SpecCube->GetNumMips(), 1u));
				}
			}
		}
		else
		{
			d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.IBLMIpCount = 1.f;
		}
		RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, d->GET_SHADER_STRUCT_MEMBER(CBPerFrame));

		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, d->MeshMaterial->GetBaseColorTexture());
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 1, d->MeshMaterial->GetNormalTexture());
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 2, d->MeshMaterial->GetMetallicRoughnessTexture());
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 3, d->MeshMaterial->GetEmissiveTexture());
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 4, d->MeshMaterial->GetOcclusionTexture());

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

		SetPipeLineState(RHIContext, RenderParam.TargetBuffer);
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

}