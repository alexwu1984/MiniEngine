#include "Engine/Render/PBRMaterialRender.h"
#include "RHI/RHIShdader.h"
#include "RHI/DynamicRHI.h"
#include "Engine.h"
#include "GltfModel/GltfMesh.h"
#include "GltfModel/GltfMaterial.h"
#include "GltfModel/GltfMeshBuffer.h"
#include "Thread/RenderThread.h"

namespace Engine
{
	using namespace RenderCore;
	struct PBRMaterialRenderP
	{
		std::shared_ptr<GltfMesh> Mesh;
		std::shared_ptr< RHIVertexShader> VertexShader;
		std::shared_ptr< RHIPixelShader> PixelShader;
	};

	PBRMaterialRender::PBRMaterialRender(std::shared_ptr<GltfMesh> Mesh)
		:Impl(std::make_shared<PBRMaterialRenderP>())
	{
		Impl->Mesh = Mesh;
	}

	PBRMaterialRender::~PBRMaterialRender()
	{

	}

	void PBRMaterialRender::InitRenderResource(nlohmann::json& jsonObj)
	{
	
	}

	void PBRMaterialRender::InitShader(const std::wstring& Path)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND(([Impl = Impl,Path](RenderCore::DynamicRHI* RHI){

			std::wstring ShaderPath = Path + L"PBRMaterial.hlsl";

			const auto& VerticesBuffer = Impl->Mesh->GetMeshBuffer()->GetVerticesBuffer();
			RHIVertexDeclare VertexDeclareRHI;
			VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(0, EVertexElementType::VET_Float3, false));
			VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(1, EVertexElementType::VET_Float3, false));
			VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(2, EVertexElementType::VET_Float2, false));

			std::vector< RHIShaderMacro> ShaderMacros;
			int32_t Index = 2;
			if (VerticesBuffer[GltfMeshBuffer::VT_Tangent])
			{
				VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(++Index, EVertexElementType::VET_Float3, false));
			}
			
			if (VerticesBuffer[GltfMeshBuffer::VT_JointsWeights0])
			{
				VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(++Index, EVertexElementType::VET_Float3, false));
			}

			if (VerticesBuffer[GltfMeshBuffer::VT_JointsIndices0])
			{
				VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(++Index, EVertexElementType::VET_Float3, false));
			}

			Impl->VertexShader = RHI->RHICreateVertexShader(ShaderPath, "MainVS", VertexDeclareRHI, ShaderMacros);
			Impl->PixelShader = RHI->RHICreatePixelShader(ShaderPath, "PBRMainPS", {});
		}))
		

	}

}