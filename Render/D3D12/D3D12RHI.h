#pragma once
#include "RHI/DynamicRHI.h"
#include "win/com_ptr.h"
#include "d3dx12.h"

namespace RenderCore
{
	class D3D12DynamicRHI;
	class FD3D12Adapter;
	class FD3D12Device;

	class D3D12DynamicRHIModule : public IDynamicRHIModule
	{
	public:
		D3D12DynamicRHIModule() = default;
		~D3D12DynamicRHIModule() = default;
		bool IsSupported() override;
		std::shared_ptr<DynamicRHI> CreateRHI() override;
	protected:
		std::shared_ptr< D3D12DynamicRHI> _DynamicRHI;
		std::vector<std::shared_ptr<FD3D12Adapter>> _ChosenAdapters;

		// set MaxSupportedFeatureLevel and ChosenAdapter
		void FindAdapter();
	};

	class D3D12DynamicRHI : public std::enable_shared_from_this<D3D12DynamicRHI>, public DynamicRHI
	{
	public:
		D3D12DynamicRHI(std::shared_ptr<FD3D12Adapter> InAdapter);
		virtual ~D3D12DynamicRHI();

		/** Initializes the RHI; separate from IDynamicRHIModule::CreateRHI so that GDynamicRHI is set when it is called. */
		virtual void Init() override;

		/** Shutdown the RHI; handle shutdown and resource destruction before the RHI's actual destructor is called (so that all resources of the RHI are still available for shutdown). */
		virtual void Shutdown() override;

		virtual const TCHAR* GetName() { return TEXT("D3D12"); }

		virtual std::shared_ptr<RHICommandContext> GetDefaultCommandContext() override;
		virtual std::shared_ptr<RHICommandContext> GetDefaultAsyncComputeContext() override;
		win32::com_ptr<ID3D12CommandQueue> CreateCommandQueue(std::weak_ptr<FD3D12Device> Device, const D3D12_COMMAND_QUEUE_DESC& Desc);

		virtual std::shared_ptr< RHIViewPort> RHICreateViewport(void* WindowHandle, uint32_t SizeX, uint32_t SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat) override;
		virtual std::shared_ptr<RHIVertexBuffer> RHICreateVertexBuffer(const void* InData, EBufferUsageFlags InUsage, int32_t StrideByteWidth, int32_t Count) override;
		virtual void RHIUpdateVertexBuffer(std::shared_ptr< RHIVertexBuffer> VertexBuffer, const void* InData, int32_t nVertex, int32_t sizePerVertex) override;
		virtual std::shared_ptr<RHIIndexBuffer> RHICreateIndexBuffer(const uint16_t* InData, EBufferUsageFlags InUsage, int32_t IndexCount) override;
		virtual std::shared_ptr<RHIIndexBuffer> RHICreateIndexBuffer(const uint32_t* InData, EBufferUsageFlags InUsage, int32_t IndexCount) override;

		virtual std::shared_ptr<RHIUniformBuffer> RHICreateUniformBuffer(uint32_t ConstantBufferSize) override;
		virtual std::shared_ptr<RHIUniformBuffer> RHICreateUniformBuffer(const void* Contents, uint32_t ConstantBufferSize) override;

		virtual std::shared_ptr<RHITexture2D> RHICreateTexture2D(EPixelFormat format, int32_t Flags, int32_t width, int32_t height, uint32_t NumMips, void* pBuffer = nullptr, int rowBytes = 0) override;
		virtual std::shared_ptr<RHITexture2D> RHICreateTexture2D(const std::wstring& FileName) override;
		virtual std::shared_ptr<RHITexture2D> RHICreateTexture2D(const core::FLinearColor& Color) override;
		virtual std::shared_ptr<RHITexture2D> RHICreateHDRTexture2D(const std::wstring& FileName) override;
		virtual std::shared_ptr<RHITexture1D> RHICreateTexture1D(EPixelFormat Format, int32_t Flags, int32_t SizeX, void* InBuffer, int RowBytes) override;
		virtual std::shared_ptr<RHITextureCube> RHICreateTextureCube(EPixelFormat Format, int32_t SizeX, int32_t SizeY, uint32_t NumMips, bool CreateDepth) override;
		virtual std::shared_ptr<RHIUnorderedAccessView> RHICreateUnorderedAccessView(EPixelFormat Format, int32_t SizeX, int32_t SizeY) override;
		virtual std::shared_ptr<RHIUnorderedAccessView> RHICreateUnorderedAccessView(std::shared_ptr< RHITexture2D> Tex2D) override;

		virtual std::shared_ptr<RHIRenderTarget> RHICreateRenderTarget(EPixelFormat Format, int32_t SizeX, int32_t SizeY, uint32_t NumMips, bool IsMultiSampled, bool CreateDepth) override;

		virtual std::shared_ptr<RHIVertexShader> RHICreateVertexShader(const std::wstring& FileName, const std::string& VSMain, const RHIVertexDeclare& VertexDeclare, 
																	   const std::vector<RHIShaderMacro>& MacroDefines) override;
		virtual std::shared_ptr<RHIPixelShader> RHICreatePixelShader(const std::wstring& FileName, const std::string& PSMain, const std::vector<RHIShaderMacro>& MacroDefines) override;
		virtual std::shared_ptr<RHIComputeShader> RHICreateComputeShader(const std::wstring& FileName, const std::string& CSMain, const std::vector<RHIShaderMacro>& MacroDefines) override;

		virtual std::shared_ptr<RHISamplerState> RHICreateSamplerState(const SamplerStateInitializerRHI& Initializer) override;
		virtual std::shared_ptr<RHIRasterizerState> RHICreateRasterizerState(const RasterizerStateInitializerRHI& Initializer) override;
		virtual std::shared_ptr<RHIBlendState> RHICreateBlendState(const BlendStateInitializerRHI& Initializer) override;
		virtual std::shared_ptr<RHIDepthStencilState> RHICreateDepthStencilState(const DepthStencilStateInitializerRHI& Initializer) override;
		virtual std::shared_ptr<RHITilePool> RHICreateTilePool(std::shared_ptr< RHITexture2D> Tex2D) override;

	private:
		std::shared_ptr<FD3D12Adapter> D3D12Adapter;
		std::unordered_map<std::wstring, std::shared_ptr<RHITexture2D>> TexCaches;
	};
}