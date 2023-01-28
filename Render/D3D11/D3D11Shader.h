#pragma once
#include "RHI/RHIShdader.h"
#include "RHIPrivate/D3D11RHIDeclare.h"

namespace RenderCore
{
	class D3D11DynamicRHI;
	struct D3D11VertexShaderP;
	struct D3D11PixelShaderP;

	class D3D11VertexShader : public RHIVertexShader
	{
	public:
		D3D11VertexShader(D3D11DynamicRHI* D3D11RHI);
		virtual ~D3D11VertexShader();

		bool CreateShader(const std::wstring& FileName, const std::string& VSMain, const RHIVertexDeclare& VertexDeclare, const std::vector<RHIShaderMacro>& MacroDefines) override;
		ID3D11VertexShader* GetNativeVertexShader() const;
		ID3D11InputLayout* GetNativeInputLayout() const;
	private:
		bool CreateLayout(const std::vector< VertexElementDesc>& ElementDescs);
		

	private:
		std::shared_ptr<D3D11VertexShaderP> Impl;
	};

	class D3D11PixelShader : public RHIPixelShader
	{
	public:
		D3D11PixelShader(D3D11DynamicRHI* D3D11RHI);
		virtual ~D3D11PixelShader();

		bool CreateShader(const std::wstring& FileName, const std::string& PSMain, const std::vector<RHIShaderMacro>& MacroDefines) override;
		ID3D11PixelShader* GetNativePixelShader() const;
	private:
		std::shared_ptr<D3D11PixelShaderP> Impl;

	};
}