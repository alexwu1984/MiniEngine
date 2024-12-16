#pragma once
#include "RHI/RHIShdader.h"
#include "RHIPrivate/D3D12RHIPrivate.h"

namespace RenderCore
{
	/** This represents a vertex shader that hasn't been combined with a specific declaration to create a bound shader. */
	class FD3D12VertexShader : public RHIVertexShader
	{
	public:
		FD3D12VertexShader();
		virtual ~FD3D12VertexShader() = default;

		bool CreateShader(const std::wstring& FileName, const std::string& VSMain, const RHIVertexDeclare& VertexDeclare, const std::vector<RHIShaderMacro>& MacroDefines) override;

		enum { StaticFrequency = SF_Vertex };

		/** The vertex shader's bytecode, with custom data in the last byte. */
		std::vector<uint8_t> Code;

		// TEMP remove with removal of bound shader state
		int32_t Offset = 0;

		FShaderCodePackedResourceCounts ResourceCounts;
	};

	class FD3D12PixelShader : public RHIPixelShader
	{
	public:
		FD3D12PixelShader();
		virtual ~FD3D12PixelShader() = default;

		bool CreateShader(const std::wstring& FileName, const std::string& PSMain, const std::vector<RHIShaderMacro>& MacroDefines) override;
		
		enum { StaticFrequency = SF_Pixel };

		/** The shader's bytecode, with custom data in the last byte. */
		std::vector<uint8_t> Code;

		FShaderCodePackedResourceCounts ResourceCounts;

	};
}