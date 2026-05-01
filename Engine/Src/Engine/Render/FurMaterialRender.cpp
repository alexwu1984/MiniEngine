#include "Engine/Render/FurMaterialRender.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHICachedStates.h"
#include "Engine.h"
#include "Render/MaterialPreFrame.h"
#include "Material/GltfFurMaterial.h"
#include "GltfModel/GltfModelConfig.h"

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
	};

	FurMaterialRender::FurMaterialRender(std::shared_ptr<GltfMeshBuffer> MeshBuffer, std::shared_ptr< MaterialBase> MeshMaterial,
										 const FurConfig& InConifg,
										 std::shared_ptr<RenderCore::RHITexture2D> NoiseTex)
		:PBRMaterialRender(MeshBuffer,MeshMaterial)
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
	}

	std::wstring FurMaterialRender::GetShaderFileName() const
	{
		return L"FurMaterial.hlsl";
	}

	void FurMaterialRender::AddShaderMacro(std::vector<RHIShaderMacro>& ShaderMacros)
	{
		ShaderMacros.push_back({ "HASFUR","1" });
	}

	void FurMaterialRender::DrawMesh(RHICommandContext& RHIContext)
	{
		C_P(FurMaterialRender);
		RenderCore::RHICommandMark Mark(RHIContext, "FurPass");
		auto& FurConfig = d->Config;
		d->GET_UNIFORMDATA(CBPerFur).FurLength = FurConfig.FurLength;
		d->GET_UNIFORMDATA(CBPerFur).FurLevel = FurConfig.FurLevel;
		d->GET_UNIFORMDATA(CBPerFur).UVScale = FurConfig.UVScale;
		d->GET_UNIFORMDATA(CBPerFur).FurAmbientStrength = FurConfig.FurAmbientStrength;
		d->GET_UNIFORMDATA(CBPerFur).FurLightExposure = FurConfig.FurLightExposure;
		d->GET_UNIFORMDATA(CBPerFur).DrawSolid = 0;

		RHIContext.RHISetBlendState(RHICachedStates::BlendDeferredTranslucentMRT, core::FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		// Shells are blended: writing depth lets low-alpha fluff win the depth buffer so deferred reads wrong Z (dark rim).
		RHIContext.RHISetDepthStencilState(RHICachedStates::DepthStateLessEqualNoWrite, 0);
		RHIContext.RHISetRasterizerState(RHICachedStates::RasterizerStateCullBack);

		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 1, d->NoiseTex);

		for (int32_t Index = 0; Index < FurConfig.FurLevel; Index++)
		{
			float FurOffset = 1.0 / FurConfig.FurLevel * (Index + 1);
			d->GET_UNIFORMDATA(CBPerFur).FurOffset = FurOffset;

			d->GET_SHADER_STRUCT_MEMBER(CBPerFur).UpdateUniformBuffer();
			d->GET_SHADER_STRUCT_MEMBER(CBPerFur).SetShaderUniformBuffer(RenderCore::SF_Vertex);
			d->GET_SHADER_STRUCT_MEMBER(CBPerFur).SetShaderUniformBuffer(RenderCore::SF_Pixel);
			RHIContext.RHISetShaderSampler(RenderCore::SF_Vertex, 0, RenderCore::RHICachedStates::WarpLinerSampler);
			RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::WarpLinerSampler);

			DrawPrimitive(RHIContext);
		}
	}

	void FurMaterialRender::PreDrawMesh(RenderCore::RHICommandContext& RHIContext)
	{
		C_P(FurMaterialRender);
		RenderCore::RHICommandMark Mark(RHIContext, "FurPrePass");
		auto& FurConfig = d->Config;
		d->GET_UNIFORMDATA(CBPerFur).FurLength = 0;
		d->GET_UNIFORMDATA(CBPerFur).FurLevel = 0;
		d->GET_UNIFORMDATA(CBPerFur).UVScale = FurConfig.UVScale;
		d->GET_UNIFORMDATA(CBPerFur).FurAmbientStrength = FurConfig.FurAmbientStrength;
		d->GET_UNIFORMDATA(CBPerFur).FurLightExposure = FurConfig.FurLightExposure;
		d->GET_UNIFORMDATA(CBPerFur).FurOffset = 0;
		d->GET_UNIFORMDATA(CBPerFur).DrawSolid = 1;

		d->GET_SHADER_STRUCT_MEMBER(CBPerFur).UpdateUniformBuffer();
		d->GET_SHADER_STRUCT_MEMBER(CBPerFur).SetShaderUniformBuffer(RenderCore::SF_Vertex);
		d->GET_SHADER_STRUCT_MEMBER(CBPerFur).SetShaderUniformBuffer(RenderCore::SF_Pixel);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Vertex, 0, RenderCore::RHICachedStates::WarpLinerSampler);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::WarpLinerSampler);

		DrawPrimitive(RHIContext);
	}

	bool FurMaterialRender::IsNeedPreDraw() const
	{
		return true;
	}

}
