#include "RHI/RHIShdader.h"
#include <dxgiformat.h>
#include "win/win32.h"
#include "common/crc.h"

namespace RenderCore
{
	struct RHIVertexDeclareP
	{
		std::vector< VertexElementDesc> Decs;
	};

	RHIVertexDeclare::RHIVertexDeclare()
		:Data(std::make_shared<RHIVertexDeclareP>())
	{

	}

	void RHIVertexDeclare::CreateDeclare(const std::vector< VertexDeclareInput>& Inputs)
	{
		//Default create separable layout
		uint32_t index = 0;
		for (const auto& Item: Inputs)
		{
			AppendDeclareInput(Item);
		}
	}

	void RHIVertexDeclare::AppendDeclareInput(const VertexDeclareInput& DeclareInput)
	{
		VertexElementDesc D3DElement{};
		switch (DeclareInput.InElementType)
		{
		case VET_Float1:		D3DElement.Format = DXGI_FORMAT_R32_FLOAT; break;
		case VET_Float2:		D3DElement.Format = DXGI_FORMAT_R32G32_FLOAT; break;
		case VET_Float3:		D3DElement.Format = DXGI_FORMAT_R32G32B32_FLOAT; break;
		case VET_Float4:		D3DElement.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; break;
		case VET_PackedNormal:	D3DElement.Format = DXGI_FORMAT_R8G8B8A8_SNORM; break; //TODO: uint32 doesn't work because D3D11 squishes it to 0 in the IA-VS conversion
		case VET_UByte4:		D3DElement.Format = DXGI_FORMAT_R8G8B8A8_UINT; break; //TODO: SINT, blendindices
		case VET_UByte4N:		D3DElement.Format = DXGI_FORMAT_R8G8B8A8_UNORM; break;
		case VET_Color:			D3DElement.Format = DXGI_FORMAT_B8G8R8A8_UNORM; break;
		case VET_Short2:		D3DElement.Format = DXGI_FORMAT_R16G16_SINT; break;
		case VET_Short4:		D3DElement.Format = DXGI_FORMAT_R16G16B16A16_SINT; break;
		case VET_Short2N:		D3DElement.Format = DXGI_FORMAT_R16G16_SNORM; break;
		case VET_Half2:			D3DElement.Format = DXGI_FORMAT_R16G16_FLOAT; break;
		case VET_Half4:			D3DElement.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; break;
		case VET_Short4N:		D3DElement.Format = DXGI_FORMAT_R16G16B16A16_SNORM; break;
		case VET_UShort2:		D3DElement.Format = DXGI_FORMAT_R16G16_UINT; break;
		case VET_UShort4:		D3DElement.Format = DXGI_FORMAT_R16G16B16A16_UINT; break;
		case VET_UShort2N:		D3DElement.Format = DXGI_FORMAT_R16G16_UNORM; break;
		case VET_UShort4N:		D3DElement.Format = DXGI_FORMAT_R16G16B16A16_UNORM; break;
		case VET_URGB10A2N:		D3DElement.Format = DXGI_FORMAT_R10G10B10A2_UNORM; break;
		case VET_UInt:			D3DElement.Format = DXGI_FORMAT_R32_UINT; break;
		default: break;
		};
		strcpy_s(D3DElement.SemanticName, "ATTRIBUTE");
		D3DElement.SemanticIndex = DeclareInput.InAttributeIndex;
		D3DElement.InputSlot = (uint32_t)Data->Decs.size();
		//DeclareInput.bUseInstanceIndex ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;
		D3DElement.InputSlotClass = DeclareInput.bUseInstanceIndex ? 1 : 0;

		// This is a divisor to apply to the instance index used to read from this stream.
		D3DElement.InstanceDataStepRate = DeclareInput.bUseInstanceIndex ? 1 : 0;

		Data->Decs.emplace_back(D3DElement);
	}

	const std::vector< RenderCore::VertexElementDesc>& RHIVertexDeclare::GetDeclareDesc() const
	{
		return Data->Decs;
	}

	uint32_t RHIVertexDeclare::GetHash() const
	{
		return core::Crc::MemCrc32(Data->Decs.data(), Data->Decs.size() * sizeof(VertexElementDesc));
	}

}