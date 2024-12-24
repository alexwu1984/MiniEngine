#pragma once
#include "RHI/RHIDefinitions.h"

namespace RenderCore
{
	//enum EVertexElementSemanticType : int32_t
	//{
	//	VEST_None = -1,
	//	VEST_Position = 0,
	//	VEST_Color,
	//	VEST_TexCoord,
	//	VEST_TexCoord1,
	//	VEST_TexCoord2,
	//	VEST_TexCoord3,
	//	VEST_Normal,
	//	VEST_BlendWeight,
	//	VEST_BlendINdex,
	//	VEST_Tangent,
	//	VEST_BiNormal,
	//	/**particle*/
	//	VEST_Velocity,
	//	VEST_Size,
	//	VEST_Age,
	//	VEST_Type,
	//	VEST_MAX,
	//};

	enum EVertexElementType : int32_t
	{
		VET_None = -1,
		VET_Float1 = 0,
		VET_Float2,
		VET_Float3,
		VET_Float4,
		VET_PackedNormal,	// FPackedNormal
		VET_UByte4,
		VET_UByte4N,
		VET_Color,
		VET_Short2,
		VET_Short4,
		VET_Short2N,		// 16 bit word normalized to (value/32767.0,value/32767.0,0,0,1)
		VET_Half2,			// 16 bit float using 1 bit sign, 5 bit exponent, 10 bit mantissa 
		VET_Half4,
		VET_Short4N,		// 4 X 16 bit word, normalized 
		VET_UShort2,
		VET_UShort4,
		VET_UShort2N,		// 16 bit word normalized to (value/65535.0,value/65535.0,0,0,1)
		VET_UShort4N,		// 4 X 16 bit word unsigned, normalized 
		VET_URGB10A2N,		// 10 bit r, g, b and 2 bit a normalized to (value/1023.0f, value/1023.0f, value/1023.0f, value/3.0f)
		VET_UInt,
		VET_MAX,
	};

	struct VertexDeclareInput
	{
		//EVertexElementSemanticType SemanticType = EVertexElementSemanticType::VEST_None;
		VertexDeclareInput(uint8_t AttributeIndex, EVertexElementType ElementType, bool UseInstanceIndex)
			:InAttributeIndex(AttributeIndex)
			, InElementType(ElementType)
			, bUseInstanceIndex(UseInstanceIndex)
		{

		}

		uint8_t InAttributeIndex = 0;
		EVertexElementType InElementType = EVertexElementType::VET_None;
		bool bUseInstanceIndex = false;
	};

	struct VertexElementDesc
	{
		char SemanticName[20]{0};
		uint32_t SemanticIndex = 0;
		uint32_t Format = 0;
		uint32_t InputSlot = 0;
		uint32_t AlignedByteOffset = 0;
		int32_t InputSlotClass = 0;
		uint32_t InstanceDataStepRate = 0;
	};

	struct RHIVertexDeclareP;

	class RHIVertexDeclare
	{
	public:
		RHIVertexDeclare();
		~RHIVertexDeclare() = default;

		void CreateDeclare(const std::vector< VertexDeclareInput>& Inputs);
		void AppendDeclareInput(const VertexDeclareInput& DeclareInput);
		const std::vector< VertexElementDesc>& GetDeclareDesc() const;
		uint32_t GetHash() const;
	private:
		std::shared_ptr< RHIVertexDeclareP> Data;
	};

	struct RHIShaderMacro
	{
		std::string Name;
		std::string Definition;
	};

	class RHIGraphicShader
	{
	public:
		RHIGraphicShader(EShaderFrequency Type) :ShaderType(Type) {}
		virtual ~RHIGraphicShader() = default;
		EShaderFrequency ShaderType;
	};

	class RHIVertexShader : public RHIGraphicShader
	{
	public:
		RHIVertexShader(EShaderFrequency Type) :RHIGraphicShader(Type) {}
		virtual ~RHIVertexShader() = default;
		virtual bool CreateShader(const std::wstring& FileName, const std::string& VSMain, const RHIVertexDeclare& VertexDeclare, const std::vector<RHIShaderMacro>& MacroDefines = {}) = 0;

	};

	class RHIPixelShader : public RHIGraphicShader
	{
	public:
		RHIPixelShader(EShaderFrequency Type) :RHIGraphicShader(Type) {}
		virtual ~RHIPixelShader() = default;
		virtual bool CreateShader(const std::wstring& FileName, const std::string& PSMain, const std::vector<RHIShaderMacro>& MacroDefines = {}) = 0;
	};

	class RHIComputeShader : public RHIGraphicShader
	{
	public:
		RHIComputeShader(EShaderFrequency Type) :RHIGraphicShader(Type) {}
		virtual ~RHIComputeShader() = default;
		virtual bool CreateShader(const std::wstring& FileName, const std::string& CSMain, const std::vector<RHIShaderMacro>& MacroDefines = {}) = 0;
	};

	class RHIShaderCache
	{
	public:
		RHIShaderCache() = default;
		~RHIShaderCache() = default;
	public:
		std::unordered_map<size_t, std::shared_ptr< RHIVertexShader>> VertexShaderCache;
		std::unordered_map<size_t, std::shared_ptr< RHIPixelShader>> PixelShaderCache;
		std::unordered_map<size_t, std::shared_ptr< RHIComputeShader>> ComputeShaderCache;
	};
}