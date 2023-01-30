#include "Engine/Render/PBRMaterialRender.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIShaderDefine.h"
#include "Engine.h"
#include "GltfModel/GltfMaterial.h"
#include "GltfModel/GltfMeshBuffer.h"
#include "Thread/RenderThread.h"
#include "math/matrix4x4.h"
#include "RHI/RHIPipleLineState.h"
#include "RHI/RHICachedStates.h"
#include "core/system.h"

namespace Engine
{
	using namespace RenderCore;


	BEGIN_SHADER_STRUCT(PBRSkinMat, 2)
		DECLARE_ARRAY_PARAM(math::Matrix4x4, MAX_MATRICES, BoneMat)
		BEGIN_STRUCT_CONSTRUCT(PBRSkinMat)
		END_STRUCT_CONSTRUCT
	END_SHADER_STRUCT

	struct PBRMaterialRenderP
	{
		PBRMaterialRenderP() :GET_SHADER_STRUCT_MEMBER(PBRSkinMat)(GEngine->GetRHI().get()) {}
		std::shared_ptr<GltfMeshBuffer> MeshBuffer;
		std::shared_ptr<GltfMaterial> MeshMaterial;
		std::shared_ptr< RHIVertexShader> VertexShader;
		std::shared_ptr< RHIPixelShader> PixelShader;
		DECLARE_SHADER_STRUCT_MEMBER(PBRSkinMat)
	};

	PBRMaterialRender::PBRMaterialRender(std::shared_ptr<GltfMeshBuffer> MeshBuffer, std::shared_ptr< GltfMaterial> MeshMaterial)
		:Impl(std::make_shared<PBRMaterialRenderP>())
	{
		Impl->MeshBuffer = MeshBuffer;
		Impl->MeshMaterial = MeshMaterial;
	}

	PBRMaterialRender::~PBRMaterialRender()
	{
		
	}

	void PBRMaterialRender::InitRenderResource(nlohmann::json& jsonObj)
	{
		std::wstring ShaderPath = core::process_directory().wstring() + L"/ShaderLibDX/";
		InitShader(ShaderPath);
	}

	void PBRMaterialRender::InitShader(const std::wstring& Path)
	{
		auto InitShaderFun = [Impl = Impl, Path](RenderCore::DynamicRHI* RHI) {

			std::wstring ShaderPath = Path + L"PBRMaterial.hlsl";

			const auto& VerticesBuffer = Impl->MeshBuffer->GetVerticesBuffer();
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
				VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(++Index, EVertexElementType::VET_Float3, false));
				ShaderMacros.push_back({ "HAS_TANGENT","1" });
			}

			if (VerticesBuffer[RenderCore::VT_JointsWeights0])
			{
				VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(++Index, EVertexElementType::VET_Float3, false));
				ShaderMacros.push_back({ "HAS_WEIGHTS_0","1" });
			}

			if (VerticesBuffer[RenderCore::VT_JointsIndices0])
			{
				VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(++Index, EVertexElementType::VET_Float3, false));
			}

			Impl->VertexShader = RHI->RHICreateVertexShader(ShaderPath, "MainVS", VertexDeclareRHI, ShaderMacros);
			Impl->PixelShader = RHI->RHICreatePixelShader(ShaderPath, "PBRMainPS", {});
		};

		ENQUEUE_UNIQUE_RENDER_COMMAND(InitShaderFun)
		
	}

	void PBRMaterialRender::Draw(RenderCore::RHICommandContext& RHIContext, const MaterialRenderParam& RenderParam)
	{
		GraphicsPipelineStateInitializer Init;
		Init.PixelShader = Impl->PixelShader;
		Init.VertexShader = Impl->VertexShader;
		Init.RasterizerState = RHICachedStates::RasterizerStateCullFront;
		if (Impl->MeshMaterial->IsTransparent())
		{
			Init.BlendState = RHICachedStates::BlendTraditional;
		}
		else
		{
			Init.BlendState = RHICachedStates::BlendOnAlphaOff;
		}
		Init.DepthStencilState = RHICachedStates::DepthStateEnable;
		RHIContext.RHISetGraphicsPipelineState(Init);
		
		//to do,set uniform buffer

		//render
		RHIContext.DrawPrimitive(Impl->MeshBuffer->GetVerticesBuffer(), Impl->MeshBuffer->GetIndexBuffer());
	}

}