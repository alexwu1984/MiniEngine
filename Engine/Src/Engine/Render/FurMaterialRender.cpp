#include "Engine/Render/FurMaterialRender.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHICachedStates.h"
#include "Engine.h"
#include "Render/MaterialPreFrame.h"
#include "GltfModel/GltfFurMaterial.h"
#include "GltfModel/GltfModelConfig.h"

namespace Engine
{
	struct FurMaterialRenderPrivate
	{
		FurMaterialRenderPrivate()
			:GET_SHADER_STRUCT_MEMBER(CBPerFur)(GEngine->GetRHI().get())
		{

		}

		DECLARE_SHADER_STRUCT_MEMBER(CBPerFur);
		std::shared_ptr<GltfFurMaterial> FurMaterial;
	};

	FurMaterialRender::FurMaterialRender(std::shared_ptr<GltfMeshBuffer> MeshBuffer, std::shared_ptr< GltfMaterial> MeshMaterial)
		:PBRMaterialRender(MeshBuffer,MeshMaterial)
		,d_ptr(new FurMaterialRenderPrivate())
	{
		C_P(FurMaterialRender);
		d->FurMaterial = std::static_pointer_cast<GltfFurMaterial>(MeshMaterial);
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

	void FurMaterialRender::AddShaderMacro(std::vector<RenderCore::RHIShaderMacro>& ShaderMacros)
	{
		ShaderMacros.push_back({ "HASFUR","1" });
	}

	void FurMaterialRender::DrawMesh(RenderCore::RHICommandContext& RHIContext)
	{
		C_P(FurMaterialRender);
		auto& FurConfig = d->FurMaterial->GetFurConfig();
		d->GET_UNIFORMDATA(CBPerFur).FurLength = FurConfig.FurLength;
		d->GET_UNIFORMDATA(CBPerFur).FurLevel = FurConfig.FurLevel;
		d->GET_UNIFORMDATA(CBPerFur).UVScale = FurConfig.UVScale;
		d->GET_UNIFORMDATA(CBPerFur).FurAmbientStrength = FurConfig.FurAmbientStrength;
		d->GET_UNIFORMDATA(CBPerFur).FurLightExposure = FurConfig.FurLightExposure;

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

}
