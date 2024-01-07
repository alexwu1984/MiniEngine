#include "D3D11/D3D11Shader.h"
#include "RHI/RHIDefinitions.h"
#include "RHIPrivate/D3D11RHIPrivate.h"
#include "D3D11/D3D11RHI.h"
#include "core/logger.h"
#include "RHIPrivate/D3DShaderUtil.h"

namespace RenderCore
{
	struct D3D11VertexShaderPrivate
	{
		D3D11DynamicRHI* D3D11RHI = nullptr;
		win32::com_ptr<ID3DBlob> SharderCode;
		win32::com_ptr<ID3D11VertexShader> VertexShader;
		win32::com_ptr<ID3D11InputLayout> InputLayout;
	};

	D3D11VertexShader::D3D11VertexShader(D3D11DynamicRHI* D3D11RHI)
		:RHIVertexShader(SF_Vertex),d_ptr(new D3D11VertexShaderPrivate())
	{
		C_P(D3D11VertexShader);
		d->D3D11RHI = D3D11RHI;
	}

	D3D11VertexShader::~D3D11VertexShader()
	{
		delete d_ptr;
	}

	bool D3D11VertexShader::CreateShader(const std::wstring& FileName, const std::string& VSMain, const RHIVertexDeclare& VertexDeclare, const std::vector<RHIShaderMacro>& MacroDefines /*= {}*/)
	{
		C_P(D3D11VertexShader);
		std::vector< D3D_SHADER_MACRO> D3DShaderMacros;
		ShaderUtil::RHIShaderMarcoToD3DShaderMacro(MacroDefines, D3DShaderMacros);

		win32::com_ptr<ID3DBlob> SharderCode = ShaderUtil::CreateShader(FileName, VSMain, "vs_5_0", D3DShaderMacros.data());
		if (!SharderCode.is_valid())
		{
			return false;
		}
		d->SharderCode = SharderCode;

		auto Device = d->D3D11RHI->GetDevice();
		VERIFYD3D11RESULT(Device->CreateVertexShader(SharderCode->GetBufferPointer(), SharderCode->GetBufferSize(), nullptr, d->VertexShader.get_init_ref()));

		if (VertexDeclare.GetDeclareDesc().empty())
		{
			return d->VertexShader.is_valid();
		}

		return CreateLayout(VertexDeclare.GetDeclareDesc());
	}

	ID3D11VertexShader* D3D11VertexShader::GetNativeVertexShader() const
	{
		C_P(D3D11VertexShader);
		return d->VertexShader.get();
	}

	ID3D11InputLayout* D3D11VertexShader::GetNativeInputLayout() const
	{
		C_P(D3D11VertexShader);
		return d->InputLayout.get();
	}

	bool D3D11VertexShader::CreateLayout(const std::vector< VertexElementDesc>& ElementDescs)
	{
		C_P(D3D11VertexShader);
		if (ElementDescs.empty() )
		{
			return false;
		}

		if (!d->SharderCode.is_valid())
		{
			Assert(false);
			return false;
		}

		auto Device = d->D3D11RHI->GetDevice();

		std::vector<D3D11_INPUT_ELEMENT_DESC> D3D11ElementDescs;
		D3D11ElementDescs.resize(ElementDescs.size());

		int32_t Index = 0;
		for (const auto& Item: ElementDescs)
		{
			D3D11_INPUT_ELEMENT_DESC& ElementDesc = D3D11ElementDescs[Index++];
			ElementDesc.SemanticName = Item.SemanticName;
			ElementDesc.SemanticIndex = Item.SemanticIndex;
			ElementDesc.Format = static_cast<DXGI_FORMAT>(Item.Format);
			ElementDesc.InputSlot = Item.InputSlot;
			ElementDesc.AlignedByteOffset = Item.AlignedByteOffset;
			ElementDesc.InputSlotClass = static_cast<D3D11_INPUT_CLASSIFICATION>(Item.InputSlotClass);
			ElementDesc.InstanceDataStepRate = Item.InstanceDataStepRate;
		}

		HRESULT hr = Device->CreateInputLayout(D3D11ElementDescs.data(), (uint32_t)D3D11ElementDescs.size(), 
			d->SharderCode->GetBufferPointer(), d->SharderCode->GetBufferSize(), d->InputLayout.get_init_ref());
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

	struct D3D11PixelShaderPrivate
	{
		D3D11DynamicRHI* D3D11RHI = nullptr;
		win32::com_ptr<ID3D11PixelShader> PixelShader;
	};

	D3D11PixelShader::D3D11PixelShader(D3D11DynamicRHI* D3D11RHI)
		:RHIPixelShader(SF_Pixel), d_ptr(new D3D11PixelShaderPrivate())
	{
		C_P(D3D11PixelShader);
		d->D3D11RHI = D3D11RHI;
	}

	D3D11PixelShader::~D3D11PixelShader()
	{
		delete d_ptr;
	}

	bool D3D11PixelShader::CreateShader(const std::wstring& FileName, const std::string& PSMain, const std::vector<RHIShaderMacro>& MacroDefines )
	{
		C_P(D3D11PixelShader);
		std::vector< D3D_SHADER_MACRO> D3DShaderMacros;
		ShaderUtil::RHIShaderMarcoToD3DShaderMacro(MacroDefines, D3DShaderMacros);
		auto SharderCode = ShaderUtil::CreateShader(FileName, PSMain, "ps_5_0", D3DShaderMacros.data());
		if (!SharderCode.is_valid())
		{
			return false;
		}
		auto Device = d->D3D11RHI->GetDevice();
		HRESULT hr =  Device->CreatePixelShader(SharderCode->GetBufferPointer(), SharderCode->GetBufferSize(), nullptr, d->PixelShader.get_init_ref());
		return SUCCEEDED(hr);
	}

	ID3D11PixelShader* D3D11PixelShader::GetNativePixelShader() const
	{
		C_P(D3D11PixelShader);
		return d->PixelShader.get();
	}

	struct D3D11ComputeShaderPrivate
	{
		D3D11DynamicRHI* D3D11RHI = nullptr;
		win32::com_ptr<ID3D11ComputeShader> ComputeShader;
	};

	D3D11ComputeShader::D3D11ComputeShader(D3D11DynamicRHI* D3D11RHI)
		:RHIComputeShader(SF_Compute),d_ptr(new D3D11ComputeShaderPrivate())
	{
		C_P(D3D11ComputeShader);
		d->D3D11RHI = D3D11RHI;
	}

	D3D11ComputeShader::~D3D11ComputeShader()
	{
		delete d_ptr;
	}

	bool D3D11ComputeShader::CreateShader(const std::wstring& FileName, const std::string& CSMain, const std::vector<RHIShaderMacro>& MacroDefines)
	{
		C_P(D3D11ComputeShader);
		std::vector< D3D_SHADER_MACRO> D3DShaderMacros;
		ShaderUtil::RHIShaderMarcoToD3DShaderMacro(MacroDefines, D3DShaderMacros);
		auto SharderCode = ShaderUtil::CreateShader(FileName, CSMain, "cs_5_0", D3DShaderMacros.data());
		if (!SharderCode.is_valid())
		{
			return false;
		}
		auto Device = d->D3D11RHI->GetDevice();
		HRESULT hr = Device->CreateComputeShader(SharderCode->GetBufferPointer(), SharderCode->GetBufferSize(), nullptr, d->ComputeShader.get_init_ref());
		return SUCCEEDED(hr);
	}

	ID3D11ComputeShader* D3D11ComputeShader::GetNativeComputeShader() const
	{
		C_P(D3D11ComputeShader);
		return d->ComputeShader.get();
	}

}