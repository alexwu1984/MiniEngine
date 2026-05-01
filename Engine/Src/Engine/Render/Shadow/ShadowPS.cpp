#include "Render/Shadow/ShadowPS.h"
#include "RHI/DynamicRHI.h"
#include "Engine/Engine.h"
#include "core/system.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIShaderDefine.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "Render/MaterialPreFrame.h"
#include "Render/Blur.h"
#include "Engine/GltfModel/GltfMesh.h"
#include "Engine/GltfModel/GltfMeshBuffer.h"
#include "Engine/Material/GltfMaterial.h"
#include "RHI/RHIRenderTarget.h"

namespace Engine
{
	using namespace RenderCore;

	struct ShadowPSPrivate
	{
		ShadowPSPrivate(DynamicRHI* _RHI)
			:RHI(_RHI),
			GET_SHADER_STRUCT_MEMBER(CBPerSkeleton)(GEngine->GetRHI().get()),
			GET_SHADER_STRUCT_MEMBER(CBPerFrame)(GEngine->GetRHI().get()),
			GET_SHADER_STRUCT_MEMBER(CBPerObject)(GEngine->GetRHI().get())
		{

		}
		DynamicRHI* RHI;
		std::shared_ptr< RHIVertexShader> VertexShader;
		std::shared_ptr< RHIPixelShader> PixelShader;
		std::shared_ptr< BlurCS> Blur;
		std::shared_ptr<MeshBase> Mesh;
		bool HasSkin = false;
		DECLARE_SHADER_STRUCT_MEMBER(CBPerFrame)
		DECLARE_SHADER_STRUCT_MEMBER(CBPerObject)
		DECLARE_SHADER_STRUCT_MEMBER(CBPerSkeleton)
	};

	ShadowPS::ShadowPS(RenderCore::DynamicRHI* RHI, std::shared_ptr<MeshBase> gltfMesh)
		:d_ptr(new ShadowPSPrivate(RHI))
	{
		C_P(ShadowPS);
		d->Mesh = gltfMesh;
	}

	ShadowPS::~ShadowPS()
	{
		delete d_ptr;
	}

	void ShadowPS::InitResource()
	{
		C_P(ShadowPS);
		std::wstring ShaderPath = core::process_directory().wstring() + L"/ShaderLibDX/";
		std::wstring VSPath = ShaderPath + L"ShadowPass-VS.hlsl";

		const auto& VerticesBuffer = d->Mesh->GetMeshBuffer()->GetVerticesBuffer();
		RHIVertexDeclare VertexDeclareRHI;
		VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(0, EVertexElementType::VET_Float3, false));
		VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(1, EVertexElementType::VET_Float3, false));
		VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(2, EVertexElementType::VET_Float2, false));

		std::vector< RHIShaderMacro> ShaderMacros;
		if (VerticesBuffer[RenderCore::VT_JointsWeights0])
		{
			ShaderMacros.push_back({ "ID_SKINNING_MATRICES","2" });
			d->HasSkin = true;
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

		d->VertexShader = d->RHI->RHICreateVertexShader(VSPath, "MainVS", VertexDeclareRHI, ShaderMacros);

		std::wstring PSPath = ShaderPath + L"ShadowPass-PS.hlsl";
		d->PixelShader = d->RHI->RHICreatePixelShader(PSPath, "MainPS", ShaderMacros);
	}


	void ShadowPS::Draw(RenderCore::RHICommandContext& RHIContext, const math::Matrix4x4& WorldTransform,
		const Light& mainLight, std::shared_ptr<RenderCore::RHIRenderTarget> renderTarget)
	{
		C_P(ShadowPS);
		RenderCore::RHICommandMark Mark(RHIContext, "Shadow_Depth");

		GraphicsPipelineStateInitializer Init;
		Init.PixelShader = d->PixelShader;
		Init.VertexShader = d->VertexShader;
		Init.BlendState = RHICachedStates::BlendOnAlphaOff;
		Init.DepthStencilState = RHICachedStates::DepthStateEnable;
		Init.RasterizerState = RHICachedStates::RasterizerStateCullBack;

		RHIContext.SetRenderTarget(renderTarget);
		RHIContext.RHISetGraphicsPipelineState(Init);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RHICachedStates::ClampLinerSampler);
		//to do,set uniform buffer
		d->GET_UNIFORMDATA(CBPerObject).myPerObject_u_mCurrWorld = WorldTransform;
		d->GET_SHADER_STRUCT_MEMBER(CBPerObject).UpdateUniformBuffer();
		d->GET_SHADER_STRUCT_MEMBER(CBPerObject).SetShaderUniformBuffer(RenderCore::SF_Vertex);
		d->GET_SHADER_STRUCT_MEMBER(CBPerObject).SetShaderUniformBuffer(RenderCore::SF_Pixel);

		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.Lights[0] = mainLight;
		d->GET_SHADER_STRUCT_MEMBER(CBPerFrame).UpdateUniformBuffer();
		d->GET_SHADER_STRUCT_MEMBER(CBPerFrame).SetShaderUniformBuffer(RenderCore::SF_Vertex);
		d->GET_SHADER_STRUCT_MEMBER(CBPerFrame).SetShaderUniformBuffer(RenderCore::SF_Pixel);
		if (d->HasSkin)
		{
			d->GET_SHADER_STRUCT_MEMBER(CBPerSkeleton).UpdateUniformBuffer();
			d->GET_SHADER_STRUCT_MEMBER(CBPerSkeleton).SetShaderUniformBuffer(RenderCore::SF_Vertex);
			d->GET_SHADER_STRUCT_MEMBER(CBPerSkeleton).SetShaderUniformBuffer(RenderCore::SF_Pixel);
		}
		RHIContext.DrawPrimitive(d->Mesh->GetMeshBuffer()->GetVerticesBuffer(), d->Mesh->GetMeshBuffer()->GetIndexBuffer());
	}

	void ShadowPS::SetBoneMatrix(const math::Matrix4x4& Mat, int32_t Index)
	{
		C_P(ShadowPS);
		auto const& PreviousMatrix = d->GET_UNIFORMDATA(CBPerSkeleton).PerSkeleton_u_ModelMatrix[Index].Current;
		d->GET_UNIFORMDATA(CBPerSkeleton).PerSkeleton_u_ModelMatrix[Index].Previous = PreviousMatrix;
		d->GET_UNIFORMDATA(CBPerSkeleton).PerSkeleton_u_ModelMatrix[Index].Current = Mat;
	}

}