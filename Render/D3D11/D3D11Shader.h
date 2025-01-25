#pragma once
#include "RHI/RHIShdader.h"
#include "RHIPrivate/D3D11RHIDeclare.h"

namespace RenderCore
{
	class D3D11DynamicRHI;
	struct D3D11VertexShaderPrivate;
	struct D3D11PixelShaderPrivate;
	struct D3D11ComputeShaderPrivate;

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
		D3D11VertexShaderPrivate* d_ptr = nullptr;
	};

	class D3D11PixelShader : public RHIPixelShader
	{
	public:
		D3D11PixelShader(D3D11DynamicRHI* D3D11RHI);
		virtual ~D3D11PixelShader();

		bool CreateShader(const std::wstring& FileName, const std::string& PSMain, const std::vector<RHIShaderMacro>& MacroDefines) override;
		ID3D11PixelShader* GetNativePixelShader() const;
	private:
		D3D11PixelShaderPrivate* d_ptr = nullptr;
	};

	class D3D11ComputeShader : public RHIComputeShader
	{
	public:
		D3D11ComputeShader(D3D11DynamicRHI* D3D11RHI);
		virtual ~D3D11ComputeShader();

		bool CreateShader(const std::wstring& FileName, const std::string& CSMain, const std::vector<RHIShaderMacro>& MacroDefines) override;
		ID3D11ComputeShader* GetNativeComputeShader() const;
	private:
		D3D11ComputeShaderPrivate* d_ptr = nullptr;
	};
}