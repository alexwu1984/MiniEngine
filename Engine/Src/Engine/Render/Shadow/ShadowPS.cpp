#include "Render/Shadow/ShadowPS.h"
#include "RHI/DynamicRHI.h"
#include "Engine/Engine.h"
#include "core/system.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIShaderDefine.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "Render/MaterialPreFrame.h"
#include "Render/RDGUtils.h"
#include "Render/Blur.h"
#include "Engine/GltfModel/GltfMesh.h"
#include "Engine/GltfModel/GltfMeshBuffer.h"
#include "Engine/Material/GltfMaterial.h"
#include "Material/MaterialBase.h"
#include "RHI/RHIRenderTarget.h"
#include "RHI/RHITextureCube.h"

namespace Engine
{
	using namespace RenderCore;

	namespace
	{
		static bool ShadowPassUseBlendAlphaClip(const std::shared_ptr<MeshBase>& Mesh)
		{
			const auto mat = Mesh ? Mesh->GetMaterial() : nullptr;
			return mat && mat->IsTransparent() && mat->GetBaseColorTexture() != nullptr;
		}
	}

	struct ShadowPSPrivate
	{
		ShadowPSPrivate(DynamicRHI* _RHI)
			:RHI(_RHI),
			GET_SHADER_STRUCT_MEMBER(CBPerSkeleton)(GEngine->GetRHI().get()),
			GET_SHADER_STRUCT_MEMBER(CBPerFrame)(GEngine->GetRHI().get()),
			GET_SHADER_STRUCT_MEMBER(CBPerObject)(GEngine->GetRHI().get()),
			GET_SHADER_STRUCT_MEMBER(CBPerMaterial)(GEngine->GetRHI().get())
		{

		}
		DynamicRHI* RHI;
		std::shared_ptr< RHIVertexShader> VertexShader;
		std::shared_ptr< RHIPixelShader> PixelShader;
		std::shared_ptr< BlurCS> Blur;
		std::shared_ptr<MeshBase> Mesh;
		bool HasSkin = false;
		uint32_t CachedVtxFeat = 0;
		DECLARE_SHADER_STRUCT_MEMBER(CBPerFrame)
		DECLARE_SHADER_STRUCT_MEMBER(CBPerObject)
		DECLARE_SHADER_STRUCT_MEMBER(CBPerSkeleton)
		DECLARE_SHADER_STRUCT_MEMBER(CBPerMaterial)
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

		const uint32_t VtxFeat = d->Mesh->GetMeshBuffer()->GetDeclaredVertexFeatures();
		RHIVertexDeclare VertexDeclareRHI;
		VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(0, EVertexElementType::VET_Float3, false));
		VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(1, EVertexElementType::VET_Float3, false));
		VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(2, EVertexElementType::VET_Float2, false));

		std::vector< RHIShaderMacro> ShaderMacros;
		d->HasSkin = (VtxFeat & MeshBufferVertexFeatures::Skinning) != 0;
		if (d->HasSkin)
			ShaderMacros.push_back({ "ID_SKINNING_MATRICES","2" });

		int32_t Index = 2;
		VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(++Index, EVertexElementType::VET_Float4, false));

		if (VtxFeat & MeshBufferVertexFeatures::Skinning)
		{
			VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(++Index, EVertexElementType::VET_Float4, false));
			VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(++Index, EVertexElementType::VET_Float4, false));
		}

		d->VertexShader = d->RHI->RHICreateVertexShader(VSPath, "MainVS", VertexDeclareRHI, ShaderMacros);

		std::wstring PSPath = ShaderPath + L"ShadowPass-PS.hlsl";
		d->PixelShader = d->RHI->RHICreatePixelShader(PSPath, "MainPS", ShaderMacros);
		d->CachedVtxFeat = VtxFeat;
	}

	static void BindShadowMaterialPSResources(RenderCore::RHICommandContext& RHIContext, ShadowPSPrivate* d)
	{
		const auto mat = d->Mesh ? d->Mesh->GetMaterial() : nullptr;
		const bool bAlphaClip = ShadowPassUseBlendAlphaClip(d->Mesh);
		auto& M = d->GET_UNIFORMDATA(CBPerMaterial).myMaterial;
		M.Metallic = 0.f;
		M.AlphaCutoff = mat ? mat->GetMaterialAlphaCutoff() : 0.5f;
		M.AlphaMask = bAlphaClip ? 1u : 0u;
		M.MaterialShaderFlags = bAlphaClip ? kMaterialShaderFlag_ShadowAlphaClip : 0u;
		RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, d->GET_SHADER_STRUCT_MEMBER(CBPerMaterial));
		if (mat && mat->GetBaseColorTexture())
		{
			FRDGUtils::RHICmdListDeclarePixelSamplingSrvs(RHIContext, { mat->GetBaseColorTexture() });
			RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, mat->GetBaseColorTexture());
		}
	}


	void ShadowPS::Draw(RenderCore::RHICommandContext& RHIContext, const math::Matrix4x4& WorldTransform,
		const Light& mainLight, std::shared_ptr<RenderCore::RHIRenderTarget> renderTarget)
	{
		C_P(ShadowPS);
		const uint32_t VtxFeatNow = d->Mesh && d->Mesh->GetMeshBuffer()
			? d->Mesh->GetMeshBuffer()->GetDeclaredVertexFeatures()
			: 0u;
		if (VtxFeatNow != d->CachedVtxFeat)
			InitResource();
		RenderCore::RHICommandMark Mark(RHIContext, "Shadow_Depth");

		GraphicsPipelineStateInitializer Init;
		Init.PixelShader = d->PixelShader;
		Init.VertexShader = d->VertexShader;
		Init.BlendState = RHICachedStates::BlendOnAlphaOff;
		Init.DepthStencilState = RHICachedStates::DepthStateEnable;
		const auto shadowMat = d->Mesh ? d->Mesh->GetMaterial() : nullptr;
		const bool bDoubleSided = shadowMat && shadowMat->IsDoubleSided();
		// Spot frustum often grazes large planes (e.g. floor): back-face cull drops receivers from the depth map.
		// Double-sided glTF / DCC meshes need both faces in shadow depth.
		Init.RasterizerState =
			(mainLight.Type == LightType_Spot || bDoubleSided) ? RHICachedStates::RasterizerStateCullNone : RHICachedStates::RasterizerStateCullBack;

		RHIContext.SetRenderTarget(renderTarget);
		RHIContext.RHISetGraphicsPipelineState(Init);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RHICachedStates::ClampLinerSampler);
		d->GET_UNIFORMDATA(CBPerObject).myPerObject_u_mCurrWorld = WorldTransform;
		RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, d->GET_SHADER_STRUCT_MEMBER(CBPerObject));

		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.LightCount = 1;
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.PrimaryDirectionalLightIndex = 0;
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.Lights[0] = mainLight;
		RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, d->GET_SHADER_STRUCT_MEMBER(CBPerFrame));
		if (d->HasSkin)
			RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, d->GET_SHADER_STRUCT_MEMBER(CBPerSkeleton));
		BindShadowMaterialPSResources(RHIContext, d);
		RHIContext.DrawPrimitive(d->Mesh->GetMeshBuffer()->GetVerticesBuffer(), d->Mesh->GetMeshBuffer()->GetIndexBuffer());
	}

	void ShadowPS::DrawCubeFace(RenderCore::RHICommandContext& RHIContext, const math::Matrix4x4& WorldTransform,
		const Light& faceLight, std::shared_ptr<RenderCore::RHITextureCube> cube, int32_t faceIndex)
	{
		C_P(ShadowPS);
		const uint32_t VtxFeatNow = d->Mesh && d->Mesh->GetMeshBuffer()
			? d->Mesh->GetMeshBuffer()->GetDeclaredVertexFeatures()
			: 0u;
		if (VtxFeatNow != d->CachedVtxFeat)
			InitResource();
		RenderCore::RHICommandMark Mark(RHIContext, "Shadow_PointCubeFace");

		GraphicsPipelineStateInitializer Init;
		Init.PixelShader = d->PixelShader;
		Init.VertexShader = d->VertexShader;
		Init.BlendState = RHICachedStates::BlendOnAlphaOff;
		Init.DepthStencilState = RHICachedStates::DepthStateEnable;
		const auto cubeMat = d->Mesh ? d->Mesh->GetMaterial() : nullptr;
		const bool bDoubleSided = cubeMat && cubeMat->IsDoubleSided();
		Init.RasterizerState =
			(faceLight.Type == LightType_Spot || faceLight.Type == LightType_Point || bDoubleSided) ? RHICachedStates::RasterizerStateCullNone
																								   : RHICachedStates::RasterizerStateCullBack;

		RHIContext.SetRenderTarget(cube, faceIndex, 0);
		RHIContext.RHISetGraphicsPipelineState(Init);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RHICachedStates::ClampLinerSampler);
		d->GET_UNIFORMDATA(CBPerObject).myPerObject_u_mCurrWorld = WorldTransform;
		RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, d->GET_SHADER_STRUCT_MEMBER(CBPerObject));

		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.LightCount = 1;
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.PrimaryDirectionalLightIndex = -1;
		d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.Lights[0] = faceLight;
		RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, d->GET_SHADER_STRUCT_MEMBER(CBPerFrame));
		if (d->HasSkin)
			RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, d->GET_SHADER_STRUCT_MEMBER(CBPerSkeleton));
		BindShadowMaterialPSResources(RHIContext, d);
		RHIContext.DrawPrimitive(d->Mesh->GetMeshBuffer()->GetVerticesBuffer(), d->Mesh->GetMeshBuffer()->GetIndexBuffer());
	}

	void ShadowPS::SetBoneMatrix(const math::Matrix4x4& Mat, int32_t Index)
	{
		C_P(ShadowPS);
		if (Index < 0 || Index >= CBPerSkeleton::kPaletteMatrixCount)
			return;
		auto const& PreviousMatrix = d->GET_UNIFORMDATA(CBPerSkeleton).PerSkeleton_u_ModelMatrix[Index].Current;
		d->GET_UNIFORMDATA(CBPerSkeleton).PerSkeleton_u_ModelMatrix[Index].Previous = PreviousMatrix;
		d->GET_UNIFORMDATA(CBPerSkeleton).PerSkeleton_u_ModelMatrix[Index].Current = Mat;
	}

	void ShadowPS::ResetSkeletonPaletteIdentity()
	{
		C_P(ShadowPS);
		math::Matrix4x4 I;
		I.Identity();
		auto& Data = d->GET_UNIFORMDATA(CBPerSkeleton);
		for (int i = 0; i < CBPerSkeleton::kPaletteMatrixCount; ++i)
		{
			Data.PerSkeleton_u_ModelMatrix[i].Current = I;
			Data.PerSkeleton_u_ModelMatrix[i].Previous = I;
		}
	}

}