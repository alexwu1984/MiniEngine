#include "D3D11/D3D11Shader.h"
#include "RHI/RHIDefinitions.h"
#include "RHIPrivate/D3D11RHIPrivate.h"
#include "D3D11/D3D11RHI.h"
#include "core/logger.h"
#include "RHIPrivate/D3DShaderUtil.h"
#include "RHIPrivate/ShaderPrecompileUtil.h"
#include <d3dcompiler.h>
#include <cstring>
#include <iomanip>
#include <vector>

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

		win32::com_ptr<ID3DBlob> SharderCode;
		std::vector<uint8_t> precompiledBC;
		if (TryLoadPrecompiledShaderBytecode(FileName, VSMain, "vs_5_0", MacroDefines, precompiledBC))
		{
			HRESULT hrBlob = D3DCreateBlob(precompiledBC.size(), SharderCode.get_init_ref());
			if (FAILED(hrBlob) || !SharderCode.is_valid())
			{
				core::err() << "[Shader] D3DCreateBlob failed (D3D11 VS precompiled) hr=0x" << std::hex << std::uppercase
							<< static_cast<unsigned long>(hrBlob) << std::dec << " bytes=" << precompiledBC.size()
							<< " entry=" << VSMain << " file=" << FileName;
				return false;
			}
			std::memcpy(SharderCode->GetBufferPointer(), precompiledBC.data(), precompiledBC.size());
		}
		else
		{
			SharderCode = ShaderUtil::CreateShader(FileName, VSMain, "vs_5_0", D3DShaderMacros.data());
			if (!SharderCode.is_valid())
			{
				core::err() << "[Shader] vertex shader compile failed (D3D11, see [ShaderCompile]) entry=" << VSMain << " file=" << FileName;
				return false;
			}
		}
		d->SharderCode = SharderCode;

		auto Device = d->D3D11RHI->GetDevice();
		HRESULT hrVS = Device->CreateVertexShader(SharderCode->GetBufferPointer(), SharderCode->GetBufferSize(), nullptr, d->VertexShader.get_init_ref());
		VERIFYD3DRESULT(hrVS);
		if (FAILED(hrVS) || !d->VertexShader.is_valid())
		{
			core::err() << "[Shader] ID3D11Device::CreateVertexShader failed hr=0x" << std::hex << std::uppercase << static_cast<unsigned long>(hrVS)
						<< std::dec << " entry=" << VSMain << " file=" << FileName;
			return false;
		}

		if (VertexDeclare.GetDeclareDesc().empty())
		{
			return true;
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

		win32::com_ptr<ID3DBlob> SharderCode;
		std::vector<uint8_t> precompiledBC;
		if (TryLoadPrecompiledShaderBytecode(FileName, PSMain, "ps_5_0", MacroDefines, precompiledBC))
		{
			HRESULT hrBlob = D3DCreateBlob(precompiledBC.size(), SharderCode.get_init_ref());
			if (FAILED(hrBlob) || !SharderCode.is_valid())
			{
				core::err() << "[Shader] D3DCreateBlob failed (D3D11 PS precompiled) hr=0x" << std::hex << std::uppercase
							<< static_cast<unsigned long>(hrBlob) << std::dec << " bytes=" << precompiledBC.size()
							<< " entry=" << PSMain << " file=" << FileName;
				return false;
			}
			std::memcpy(SharderCode->GetBufferPointer(), precompiledBC.data(), precompiledBC.size());
		}
		else
		{
			SharderCode = ShaderUtil::CreateShader(FileName, PSMain, "ps_5_0", D3DShaderMacros.data());
			if (!SharderCode.is_valid())
			{
				core::err() << "[Shader] pixel shader compile failed (D3D11, see [ShaderCompile]) entry=" << PSMain << " file=" << FileName;
				return false;
			}
		}

		auto Device = d->D3D11RHI->GetDevice();
		HRESULT hr =  Device->CreatePixelShader(SharderCode->GetBufferPointer(), SharderCode->GetBufferSize(), nullptr, d->PixelShader.get_init_ref());
		if (FAILED(hr))
		{
			core::err() << "[Shader] ID3D11Device::CreatePixelShader failed hr=0x" << std::hex << std::uppercase << static_cast<unsigned long>(hr)
						<< std::dec << " entry=" << PSMain << " file=" << FileName;
			return false;
		}
		return true;
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

		win32::com_ptr<ID3DBlob> SharderCode;
		std::vector<uint8_t> precompiledBC;
		if (TryLoadPrecompiledShaderBytecode(FileName, CSMain, "cs_5_0", MacroDefines, precompiledBC))
		{
			HRESULT hrBlob = D3DCreateBlob(precompiledBC.size(), SharderCode.get_init_ref());
			if (FAILED(hrBlob) || !SharderCode.is_valid())
			{
				core::err() << "[Shader] D3DCreateBlob failed (D3D11 CS precompiled) hr=0x" << std::hex << std::uppercase
							<< static_cast<unsigned long>(hrBlob) << std::dec << " bytes=" << precompiledBC.size()
							<< " entry=" << CSMain << " file=" << FileName;
				return false;
			}
			std::memcpy(SharderCode->GetBufferPointer(), precompiledBC.data(), precompiledBC.size());
		}
		else
		{
			SharderCode = ShaderUtil::CreateShader(FileName, CSMain, "cs_5_0", D3DShaderMacros.data());
			if (!SharderCode.is_valid())
			{
				core::err() << "[Shader] compute shader compile failed (D3D11, see [ShaderCompile]) entry=" << CSMain << " file=" << FileName;
				return false;
			}
		}

		auto Device = d->D3D11RHI->GetDevice();
		HRESULT hr = Device->CreateComputeShader(SharderCode->GetBufferPointer(), SharderCode->GetBufferSize(), nullptr, d->ComputeShader.get_init_ref());
		if (FAILED(hr))
		{
			core::err() << "[Shader] ID3D11Device::CreateComputeShader failed hr=0x" << std::hex << std::uppercase << static_cast<unsigned long>(hr)
						<< std::dec << " entry=" << CSMain << " file=" << FileName;
			return false;
		}
		return true;
	}

	ID3D11ComputeShader* D3D11ComputeShader::GetNativeComputeShader() const
	{
		C_P(D3D11ComputeShader);
		return d->ComputeShader.get();
	}

}