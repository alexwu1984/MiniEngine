#pragma once
#include "RHI/RHIShdader.h"
#include "RHIPrivate/D3D12RHIPrivate.h"

namespace RenderCore
{
	/** This represents a vertex shader that hasn't been combined with a specific declaration to create a bound shader. */
	class FD3D12VertexShader : public RHIVertexShader,std::enable_shared_from_this<FD3D12VertexShader>
	{
	public:
		FD3D12VertexShader();
		virtual ~FD3D12VertexShader() = default;

		bool CreateShader(const std::wstring& FileName, const std::string& VSMain, const RHIVertexDeclare& VertexDeclare, const std::vector<RHIShaderMacro>& MacroDefines) override;

		enum { StaticFrequency = SF_Vertex };
		std::string KeyName;
		/** The vertex shader's bytecode, with custom data in the last byte. */
		win32::com_ptr<ID3DBlob> Code;

		// TEMP remove with removal of bound shader state
		int32_t Offset = 0;
		std::vector<VertexElementDesc> ElementDescs;
		uint32_t Hash = 0;
		FShaderCodePackedResourceCounts ResourceCounts;
	};

	class FD3D12PixelShader : public RHIPixelShader, std::enable_shared_from_this<FD3D12PixelShader>
	{
	public:
		FD3D12PixelShader();
		virtual ~FD3D12PixelShader() = default;

		bool CreateShader(const std::wstring& FileName, const std::string& PSMain, const std::vector<RHIShaderMacro>& MacroDefines) override;
		
		enum { StaticFrequency = SF_Pixel };
		std::string KeyName;
		std::string PSEntryPoint;
		/** The shader's bytecode, with custom data in the last byte. */
		win32::com_ptr<ID3DBlob> Code;
		uint32_t Hash = 0;
		FShaderCodePackedResourceCounts ResourceCounts;
	};

	class FD3D12ComputeShader : public RHIComputeShader
	{
	public:
		FD3D12ComputeShader();
		virtual ~FD3D12ComputeShader() = default;

		bool CreateShader(const std::wstring& FileName, const std::string& CSMain, const std::vector<RHIShaderMacro>& MacroDefines) override;
		
		enum { StaticFrequency = SF_Compute };
		std::string KeyName;
		std::string CSEntryPoint;
		/** The shader's bytecode, with custom data in the last byte. */
		win32::com_ptr<ID3DBlob> Code;
		uint32_t Hash = 0;
		FShaderCodePackedResourceCounts ResourceCounts;
	};
}