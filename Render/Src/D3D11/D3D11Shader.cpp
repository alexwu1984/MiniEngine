#include "D3D11/D3D11Shader.h"
#include "RHI/RHIDefinitions.h"
#include "RHIPrivate/D3D11RHIPrivate.h"
#include "D3D11/D3D11RHI.h"
#include "core/logger.h"

namespace RenderCore
{
	struct D3D11VertexShaderP
	{
		D3D11DynamicRHI* D3D11RHI = nullptr;
		win32::com_ptr<ID3DBlob> VSBlob;
		win32::com_ptr<ID3D11InputLayout> InputLayout;
	};

	D3D11VertexShader::D3D11VertexShader(D3D11DynamicRHI* D3D11RHI)
		:Impl(std::make_shared<D3D11VertexShaderP>())
	{
		Impl->D3D11RHI = D3D11RHI;
	}

	D3D11VertexShader::~D3D11VertexShader()
	{

	}

	bool D3D11VertexShader::CreateLayout(const std::vector< VertexElementDesc>& ElementDescs)
	{
		if (ElementDescs.empty() )
		{
			return false;
		}

		if (!Impl->VSBlob.is_valid())
		{

			Assert(false);
			return false;
		}

		auto Device = Impl->D3D11RHI->GetDevice();

		std::vector<D3D11_INPUT_ELEMENT_DESC> D3D11ElementDescs;
		D3D11ElementDescs.resize(ElementDescs.size());

		int32_t Index = 0;
		for (const auto& Item: ElementDescs)
		{
			D3D11_INPUT_ELEMENT_DESC& ElementDesc = D3D11ElementDescs[Index++];
			ElementDesc.SemanticName = Item.SemanticName.c_str();
			ElementDesc.SemanticIndex = Item.SemanticIndex;
			ElementDesc.Format = static_cast<DXGI_FORMAT>(Item.Format);
			ElementDesc.InputSlot = Item.InputSlot;
			ElementDesc.AlignedByteOffset = Item.AlignedByteOffset;
			ElementDesc.InputSlotClass = static_cast<D3D11_INPUT_CLASSIFICATION>(Item.InputSlotClass);
			ElementDesc.InstanceDataStepRate = Item.InstanceDataStepRate;
		}

		HRESULT hr = Device->CreateInputLayout(D3D11ElementDescs.data(), D3D11ElementDescs.size(), 
			Impl->VSBlob->GetBufferPointer(), Impl->VSBlob->GetBufferSize(), Impl->InputLayout.get_init_ref());
		if (FAILED(hr))
		{
			core::err() << "CreateInputLayout Failed -------";
			for (const auto& Item : ElementDescs)
			{
				core::err() << "SemanticName:" << Item.SemanticName << " SemanticIndex:" << Item.SemanticIndex << " Format:" << GetD3D11TextureFormatString(static_cast<DXGI_FORMAT>(Item.Format));
			}
			Assert(false);
			return false;
		}
		return true;
	}

}