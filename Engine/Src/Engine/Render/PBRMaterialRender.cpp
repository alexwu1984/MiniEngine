#include "Engine/Render/PBRMaterialRender.h"
#include "Engine.h"
#include "GltfModel/GltfMaterial.h"
#include "GltfModel/GltfMeshBuffer.h"
#include "GltfModel/GltfModelConfig.h"
#include "Thread/RenderThread.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/RHIRenderTarget.h"
#include "core/system.h"
#include "Render/MaterialPreFrame.h"
#include "Engine/Render/PreProcessor.h"
#include "Engine/Render/IBLRender.h"
#include "Engine/Render/Shadow/ShadowRenderPass.h"
#include "Engine/Render/SceneRender.h"

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
		std::shared_ptr<GltfMaterial> MeshMaterial;
		std::shared_ptr<RHIVertexShader> VertexShader;
		std::shared_ptr<RHIPixelShader> PixelShader;
		MaterialRenderParam RenderParam;

		DECLARE_SHADER_STRUCT_MEMBER(CBPerFrame)
		DECLARE_SHADER_STRUCT_MEMBER(CBPerObject)
		DECLARE_SHADER_STRUCT_MEMBER(CBPerSkeleton)
	};

	PBRMaterialRender::PBRMaterialRender(std::shared_ptr<GltfMeshBuffer> MeshBuffer, std::shared_ptr< GltfMaterial> MeshMaterial)
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
		auto const& PreviousMatrix = d->GET_UNIFORMDATA(CBPerSkeleton).PerSkeleton_u_ModelMatrix[Index].Current;
		d->GET_UNIFORMDATA(CBPerSkeleton).PerSkeleton_u_ModelMatrix[Index].Previous = PreviousMatrix;
		d->GET_UNIFORMDATA(CBPerSkeleton).PerSkeleton_u_ModelMatrix[Index].Current = Mat;
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

			const auto& VerticesBuffer = d->MeshBuffer->GetVerticesBuffer();
			RHIVertexDeclare VertexDeclareRHI;
			VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(0, EVertexElementType::VET_Float3, false));
			VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(1, EVertexElementType::VET_Float3, false));
			VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(2, EVertexElementType::VET_Float2, false));

			std::vector< RHIShaderMacro> ShaderMacros;
			if (VerticesBuffer[RenderCore::VT_JointsWeights0])
			{
				ShaderMacros.push_back({ "ID_SKINNING_MATRICES","2" });
			}
			int32_t Index = 2;
			if (VerticesBuffer[RenderCore::VT_Tangent])
			{
				VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(++Index, EVertexElementType::VET_Float4, false));
				ShaderMacros.push_back({ "HAS_TANGENT","1" });
			}

			if (VerticesBuffer[RenderCore::VT_JointsWeights0])
			{
				VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(++Index, EVertexElementType::VET_Float4, false));
				ShaderMacros.push_back({ "HAS_WEIGHTS_0","1" });
			}

			if (VerticesBuffer[RenderCore::VT_JointsIndices0])
			{
				VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(++Index, EVertexElementType::VET_Float4, false));
			}

			AddShaderMacro(ShaderMacros);

			d->VertexShader = RHI->RHICreateVertexShader(ShaderPath, "MainVS", VertexDeclareRHI, ShaderMacros);
			d->PixelShader = RHI->RHICreatePixelShader(ShaderPath, "MainPS", ShaderMacros);
			});
		
	}

	void PBRMaterialRender::SetPipeLineState(RenderCore::RHICommandContext& RHIContext)
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

		RHIContext.RHISetGraphicsPipelineState(Init);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RHICachedStates::ClampLinerSampler);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 1, RHICachedStates::ShadowSampler);

		//to do,set uniform buffer
		d->GET_UNIFORMDATA(CBPerObject).myPerObject_u_mCurrWorld = d->RenderParam.CurrModelMatrix;
		d->GET_UNIFORMDATA(CBPerObject).myPerObject_u_mPrevWorld = d->RenderParam.PrevModelMatrix;
		d->GET_SHADER_STRUCT_MEMBER(CBPerObject).UpdateUniformBuffer();
		d->GET_SHADER_STRUCT_MEMBER(CBPerObject).SetShaderUniformBuffer(RenderCore::SF_Vertex);
		d->GET_SHADER_STRUCT_MEMBER(CBPerObject).SetShaderUniformBuffer(RenderCore::SF_Pixel);

		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.CameraPrevViewProj = d->RenderParam.PrevViewProjMatrix;
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.CameraCurrViewProj = d->RenderParam.CurrViewProjMatrix;
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.CameraCurrViewProjInverse = d->RenderParam.CurrViewProjInverseMatrix;
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.RotateIBL = d->RenderParam.RotateIBL;
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.CameraPos = d->RenderParam.CameraPos;
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.TemporalAAJitter = d->RenderParam.TemporalAAJitter;
		d->GET_SHADER_STRUCT_MEMBER(CBPerFrame).UpdateUniformBuffer();
		d->GET_SHADER_STRUCT_MEMBER(CBPerFrame).SetShaderUniformBuffer(RenderCore::SF_Vertex);
		d->GET_SHADER_STRUCT_MEMBER(CBPerFrame).SetShaderUniformBuffer(RenderCore::SF_Pixel);

		if (d->RenderParam.HasSkin)
		{
			d->GET_SHADER_STRUCT_MEMBER(CBPerSkeleton).UpdateUniformBuffer();
			d->GET_SHADER_STRUCT_MEMBER(CBPerSkeleton).SetShaderUniformBuffer(RenderCore::SF_Vertex);
			d->GET_SHADER_STRUCT_MEMBER(CBPerSkeleton).SetShaderUniformBuffer(RenderCore::SF_Pixel);
		}
	}

	void PBRMaterialRender::Draw(RenderCore::RHICommandContext& RHIContext, const MaterialRenderParam& RenderParam)
	{
		C_P(PBRMaterialRender);

		d->RenderParam = RenderParam;
		SetPipeLineState(RHIContext);

		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.LightCount = (int32_t)RenderParam.lightInfos.size();
		for (int32_t index = 0; index < RenderParam.lightInfos.size(); ++index)
		{
			d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.Lights[index] = RenderParam.lightInfos[index];
		}

		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.Material.Metallic = d->MeshMaterial->GetMaterialConfig().Metallic;

		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, d->MeshMaterial->GetBaseColorTexture());
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 1, d->MeshMaterial->GetNormalTexture());
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 2, d->MeshMaterial->GetMetallicRoughnessTexture());
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 3, d->MeshMaterial->GetEmissiveTexture());
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 4, d->MeshMaterial->GetOcclusionTexture());

		if (!RenderParam.preProcessor.expired())
		{
			auto IBL = RenderParam.preProcessor.lock()->GetIBLRender();
			RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 5, IBL->GetIrrCube());
			RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 6, IBL->GetPreIntegrateBRDF());
			RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 7, IBL->GetPreFilterCube());
		}

		if (RenderParam.lightInfos.size() > 0 && RenderParam.lightInfos[0].ShadowMapIndex >= 0)
		{
			auto shadowMap = GEngine->GetSceneRender()->GetShadowRenderPass()->GetShadowMap();
			if (shadowMap)
			{
				RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 8, shadowMap->GetTex());
			}
		}
		
		DrawMesh(RHIContext);
	}

	void PBRMaterialRender::PreDraw(RenderCore::RHICommandContext& RHIContext, const MaterialRenderParam& RenderParam)
	{
		C_P(PBRMaterialRender);
		if (!IsNeedPreDraw())
		{
			return;
		}

		d->RenderParam = RenderParam;

		SetPipeLineState(RHIContext);
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, d->MeshMaterial->GetBaseColorTexture());
		PreDrawMesh(RHIContext);
	}

	void PBRMaterialRender::DrawPrimitive(RenderCore::RHICommandContext& RHIContext)
	{
		C_P(PBRMaterialRender);
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