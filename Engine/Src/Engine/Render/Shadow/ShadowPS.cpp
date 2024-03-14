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
#include "Engine/GltfModel/GltfMaterial.h"

namespace Engine
{
	using namespace RenderCore;

	struct ShadowPSPrivate
	{
		ShadowPSPrivate(RenderCore::DynamicRHI* _RHI)
			:RHI(_RHI)
		{

		}
		RenderCore::DynamicRHI* RHI;
		std::shared_ptr< RenderCore::RHIVertexShader> VS;
		std::shared_ptr< RenderCore::RHIPixelShader> PS;
		std::shared_ptr< BlurCS> Blur;
		std::shared_ptr<GltfMesh> Mesh;
	};

	ShadowPS::ShadowPS(RenderCore::DynamicRHI* RHI, std::shared_ptr<GltfMesh> gltfMesh)
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

		if (d->Mesh->GetMaterial() && d->Mesh->GetMaterial()->GetMaterialType() == GltfMaterial::MaterialType::FUR)
		{
			ShaderMacros.push_back({ "HASFUR","1" });
		}
		d->VS = d->RHI->RHICreateVertexShader(VSPath, "MainVS", VertexDeclareRHI, ShaderMacros);

		std::wstring PSPath = ShaderPath + L"ShadowPass-PS.hlsl";
		d->PS = d->RHI->RHICreatePixelShader(PSPath, "MainPS", ShaderMacros);
	}

	void ShadowPS::Draw(RenderCore::RHICommandContext& RHIContext)
	{

	}

}