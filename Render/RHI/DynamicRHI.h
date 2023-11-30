#pragma once
#include "win/win32.h"
#include "RHI/RHIDefinitions.h"
#include "core/color.h"

namespace RenderCore
{
	enum class RHIAPIType
	{
		E_D3D11,
		E_D3D12,
	};

	class RHICommandContext;
	class RHIViewPort;
	class RHIVertexBuffer;
	class RHIIndexBuffer;
	class RHIUniformBuffer;
	class RHITexture2D;
	class RHITexture1D;
	class RHIRenderTarget;
	class RHIVertexShader;
	class RHIPixelShader;
	class RHIComputeShader;
	class RHIVertexDeclare;
	struct RHIShaderMacro;
	struct SamplerStateInitializerRHI;
	class RHISamplerState;
	struct RasterizerStateInitializerRHI;
	class RHIRasterizerState;
	class BlendStateInitializerRHI;
	class RHIBlendState;
	struct DepthStencilStateInitializerRHI;
	class RHIDepthStencilState;
	class RHITextureCube;
	class RHIUnorderedAccessView;


	class DynamicRHI
	{
	public:
		DynamicRHI() = default;
		virtual ~DynamicRHI();

		/** Initializes the RHI; separate from IDynamicRHIModule::CreateRHI so that GDynamicRHI is set when it is called. */
		virtual void Init() = 0;

		/** Shutdown the RHI; handle shutdown and resource destruction before the RHI's actual destructor is called (so that all resources of the RHI are still available for shutdown). */
		virtual void Shutdown() = 0;

		virtual const TCHAR* GetName() = 0;

		virtual std::shared_ptr< RHICommandContext> GetDefaultCommandContext() = 0;

		virtual std::shared_ptr< RHIViewPort> RHICreateViewport(void* WindowHandle, uint32_t SizeX, uint32_t SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat) { return nullptr; }
		virtual std::shared_ptr< RHIVertexBuffer> RHICreateVertexBuffer(const void* InData, EBufferUsageFlags InUsage, int32_t StrideByteWidth, int32_t Count) = 0;
		virtual void RHIUpdateVertexBuffer(std::shared_ptr< RHIVertexBuffer> VertexBuffer, const void* InData, int32_t nVertex, int32_t sizePerVertex) = 0;
		virtual std::shared_ptr< RHIIndexBuffer> RHICreateIndexBuffer(const uint16_t* InData, EBufferUsageFlags InUsage, int32_t IndexCount) = 0;
		virtual std::shared_ptr< RHIIndexBuffer> RHICreateIndexBuffer(const uint32_t* InData, EBufferUsageFlags InUsage, int32_t IndexCount) = 0;

		virtual std::shared_ptr< RHIUniformBuffer> RHICreateUniformBuffer(uint32_t ConstantBufferSize) = 0;
		virtual std::shared_ptr< RHIUniformBuffer> RHICreateUniformBuffer(const void* Contents, uint32_t ConstantBufferSize) = 0;

		virtual std::shared_ptr< RHITexture2D> RHICreateTexture2D(EPixelFormat format, int32_t Flags, int32_t width, int32_t height, void* pBuffer = nullptr, int rowBytes = 0) = 0;
		virtual std::shared_ptr< RHITexture2D> RHICreateTexture2D(const std::wstring& FileName) = 0;
		virtual std::shared_ptr< RHITexture2D> RHICreateTexture2D(const core::FLinearColor& Color) = 0;
		virtual std::shared_ptr< RHITexture2D> RHICreateHDRTexture2D(const std::wstring& FileName) = 0;
		virtual std::shared_ptr< RHITexture1D> RHICreateTexture1D(EPixelFormat Format, int32_t Flags, int32_t SizeX, void* InBuffer, int RowBytes) = 0;
		virtual std::shared_ptr< RHITextureCube> RHICreateTextureCube(EPixelFormat Format, int32_t SizeX, int32_t SizeY, uint32_t NumMips, bool CreateDepth) = 0;
		virtual std::shared_ptr< RHIUnorderedAccessView> RHICreateUnorderedAccessView(EPixelFormat Format, int32_t SizeX, int32_t SizeY) = 0;
		virtual std::shared_ptr< RHIUnorderedAccessView> RHICreateUnorderedAccessView(std::shared_ptr< RHITexture2D> Tex2D) = 0;

		virtual std::shared_ptr< RHIRenderTarget> RHICreateRenderTarget(EPixelFormat Format, int32_t SizeX, int32_t SizeY, bool IsMultiSampled, bool CreateDepth) = 0;

		virtual std::shared_ptr< RHIVertexShader> RHICreateVertexShader(const std::wstring& FileName, const std::string& VSMain, const RHIVertexDeclare& VertexDeclare, const std::vector<RHIShaderMacro>& MacroDefines ) = 0;
		virtual std::shared_ptr< RHIPixelShader> RHICreatePixelShader(const std::wstring& FileName, const std::string& PSMain, const std::vector<RHIShaderMacro>& MacroDefines) = 0;
		virtual std::shared_ptr< RHIComputeShader> RHICreateComputeShader(const std::wstring& FileName, const std::string& CSMain, const std::vector<RHIShaderMacro>& MacroDefines) = 0;

		virtual std::shared_ptr< RHISamplerState> RHICreateSamplerState(const SamplerStateInitializerRHI& Initializer) = 0;
		virtual std::shared_ptr< RHIRasterizerState> RHICreateRasterizerState(const RasterizerStateInitializerRHI& Initializer) = 0;
		virtual std::shared_ptr< RHIBlendState> RHICreateBlendState(const BlendStateInitializerRHI& Initializer) = 0;
		virtual std::shared_ptr< RHIDepthStencilState> RHICreateDepthStencilState(const DepthStencilStateInitializerRHI& Initializer) = 0;
	
	};

	bool IsRHIDeviceAMD();

	// to trigger GPU specific optimizations and fallbacks
	bool IsRHIDeviceIntel();

	// to trigger GPU specific optimizations and fallbacks
	bool IsRHIDeviceNVIDIA();

	/**
*	Each platform that utilizes dynamic RHIs should implement this function
*	Called to create the instance of the dynamic RHI.
*/
	std::shared_ptr<DynamicRHI> PlatformCreateDynamicRHI(RHIAPIType apiType);

	extern uint32_t GRHIVendorId;
	extern std::wstring GRHIAdapterName;
	extern uint32_t GRHIDeviceId;
	extern uint32_t GRHIDeviceRevision;
}

