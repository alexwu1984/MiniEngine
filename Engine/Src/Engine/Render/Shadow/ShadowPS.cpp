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
		bool ShadowBlendAlphaClip = false;
		uint32_t CachedVtxFeat = 0;
		bool CachedShadowBlendAlphaClip = false;
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

		if (VtxFeat & MeshBufferVertexFeatures::Skinning)
		{
			VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(++Index, EVertexElementType::VET_Float4, false));
		}

		d->ShadowBlendAlphaClip = ShadowPassUseBlendAlphaClip(d->Mesh);
		if (d->ShadowBlendAlphaClip)
			ShaderMacros.push_back({ "SHADOW_ALPHA_CLIP", "1" });

		d->VertexShader = d->RHI->RHICreateVertexShader(VSPath, "MainVS", VertexDeclareRHI, ShaderMacros);

		std::wstring PSPath = ShaderPath + L"ShadowPass-PS.hlsl";
		d->PixelShader = d->RHI->RHICreatePixelShader(PSPath, "MainPS", ShaderMacros);
		d->CachedVtxFeat = VtxFeat;
		d->CachedShadowBlendAlphaClip = d->ShadowBlendAlphaClip;
	}

	static void BindShadowBlendAlphaClipResources(RenderCore::RHICommandContext& RHIContext, ShadowPSPrivate* d)
	{
		const auto mat = d->Mesh->GetMaterial();
		if (!mat)
			return;
		d->GET_UNIFORMDATA(CBPerMaterial).myMaterial.Metallic = 0.f;
		d->GET_UNIFORMDATA(CBPerMaterial).myMaterial.AlphaCutoff = mat->GetMaterialAlphaCutoff();
		d->GET_UNIFORMDATA(CBPerMaterial).myMaterial.AlphaMask = 1u;
		d->GET_UNIFORMDATA(CBPerMaterial).myMaterial.Padding = 0u;
		RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, d->GET_SHADER_STRUCT_MEMBER(CBPerMaterial));
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, mat->GetBaseColorTexture());
	}


	void ShadowPS::Draw(RenderCore::RHICommandContext& RHIContext, const math::Matrix4x4& WorldTransform,
		const Light& mainLight, std::shared_ptr<RenderCore::RHIRenderTarget> renderTarget)
	{
		C_P(ShadowPS);
		const uint32_t VtxFeatNow = d->Mesh && d->Mesh->GetMeshBuffer()
			? d->Mesh->GetMeshBuffer()->GetDeclaredVertexFeatures()
			: 0u;
		const bool wantBlendAlphaClip = ShadowPassUseBlendAlphaClip(d->Mesh);
		if (VtxFeatNow != d->CachedVtxFeat || wantBlendAlphaClip != d->CachedShadowBlendAlphaClip)
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
		if (d->ShadowBlendAlphaClip)
			BindShadowBlendAlphaClipResources(RHIContext, d);
		RHIContext.DrawPrimitive(d->Mesh->GetMeshBuffer()->GetVerticesBuffer(), d->Mesh->GetMeshBuffer()->GetIndexBuffer());
	}

	void ShadowPS::DrawCubeFace(RenderCore::RHICommandContext& RHIContext, const math::Matrix4x4& WorldTransform,
		const Light& faceLight, std::shared_ptr<RenderCore::RHITextureCube> cube, int32_t faceIndex)
	{
		C_P(ShadowPS);
		const uint32_t VtxFeatNow = d->Mesh && d->Mesh->GetMeshBuffer()
			? d->Mesh->GetMeshBuffer()->GetDeclaredVertexFeatures()
			: 0u;
		const bool wantBlendAlphaClip = ShadowPassUseBlendAlphaClip(d->Mesh);
		if (VtxFeatNow != d->CachedVtxFeat || wantBlendAlphaClip != d->CachedShadowBlendAlphaClip)
			InitResource();
		RenderCore::RHICommandMark Mark(RHIContext, "Shadow_PointCubeFace");

		GraphicsPipelineStateInitializer Init;
		Init.PixelShader = d->PixelShader;
		Init.VertexShader = d->VertexShader;
		Init.BlendState = RHICachedStates::BlendOnAlphaOff;
		Init.DepthStencilState = RHICachedStates::DepthStateDisable;
		const auto cubeMat = d->Mesh ? d->Mesh->GetMaterial() : nullptr;
		Init.RasterizerState =
			(cubeMat && cubeMat->IsDoubleSided()) ? RHICachedStates::RasterizerStateCullNone : RHICachedStates::RasterizerStateCullBack;

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
		if (d->ShadowBlendAlphaClip)
			BindShadowBlendAlphaClipResources(RHIContext, d);
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