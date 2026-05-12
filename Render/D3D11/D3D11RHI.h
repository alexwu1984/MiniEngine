#pragma once
#include "RHI/DynamicRHI.h"
#include "RHIPrivate/D3D11RHIDeclare.h"

namespace RenderCore
{
	struct D3D11DynamicRHIPrivate;
	class D3D11StateCacheBase;
	class D3D11DynamicRHI;

	class D3D11DynamicRHIModule : public IDynamicRHIModule
	{
	public:
		D3D11DynamicRHIModule() = default;
		~D3D11DynamicRHIModule() = default;
		bool IsSupported() override;
		std::shared_ptr<DynamicRHI> CreateRHI() override;
	private:
		std::shared_ptr< D3D11DynamicRHI> _dynamicRHI;
	};

	class D3D11DynamicRHI : public DynamicRHI
	{
	public:
		D3D11DynamicRHI();
		virtual ~D3D11DynamicRHI();

		/** Initializes the RHI; separate from IDynamicRHIModule::CreateRHI so that GDynamicRHI is set when it is called. */
		virtual void Init() override;

		virtual void RHIFlushSubmissionPipeline() override;
		virtual void RHIWaitForGpuIdle() override;
		virtual uint32_t RHIRecommendedParallelFrameResourceSlots() const override;
		virtual void RHIBeginFrame() override;

		/** Called from D3D11 viewport Present path when Present fails or GetDeviceRemovedReason != S_OK. Logs once. */
		void NotifyFatalDeviceLossFromPresent(HRESULT hrPresent, HRESULT hrDeviceRemovedReason);

		/** Shutdown the RHI; handle shutdown and resource destruction before the RHI's actual destructor is called (so that all resources of the RHI are still available for shutdown). */
		virtual void Shutdown() override;

		virtual const TCHAR* GetName() { return TEXT("D3D11"); }

		RHIAPIType GetRHIAPIType() const override { return RHIAPIType::E_D3D11; }

		virtual std::shared_ptr< RHICommandContext> GetDefaultCommandContext() override;
		virtual std::shared_ptr< RHICommandContext> GetDefaultAsyncComputeContext() override;
		virtual std::shared_ptr< RHIViewPort> RHICreateViewport(void* WindowHandle, uint32_t SizeX, uint32_t SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat) override;
		virtual std::shared_ptr< RHIVertexBuffer> RHICreateVertexBuffer(const void* Data, EBufferUsageFlags InUsage, int32_t StrideByteWidth, int32_t Count) override;
		virtual void RHIUpdateVertexBuffer(std::shared_ptr< RHIVertexBuffer> VertexBuffer,const void* InData, int32_t nVertex, int32_t sizePerVertex) override;
		virtual std::shared_ptr< RHIIndexBuffer> RHICreateIndexBuffer(const uint16_t* InData, EBufferUsageFlags InUsage, int32_t IndexCount) override;
		virtual std::shared_ptr< RHIIndexBuffer> RHICreateIndexBuffer(const uint32_t* InData, EBufferUsageFlags InUsage, int32_t IndexCount) override;

		virtual std::shared_ptr< RHIUniformBuffer> RHICreateUniformBuffer(uint32_t ConstantBufferSize) override;
		virtual std::shared_ptr< RHIUniformBuffer> RHICreateUniformBuffer(const void* Contents, uint32_t ConstantBufferSize) override;

		virtual std::shared_ptr< RHIStructuredBuffer> RHICreateStructuredBuffer(uint32_t ElementStride, uint32_t ElementCount, EBufferUsageFlags Usage, const void* InitialData) override;

		virtual std::shared_ptr< RHITexture2D> RHICreateTexture2D(EPixelFormat Format, int32_t Flags, int32_t SizeX, int32_t SizeY, uint32_t NumMips, void* InBuffer = nullptr, int RowBytes = 0) override;
		virtual std::shared_ptr< RHITexture2D> RHICreateTexture2D(const std::wstring& FileName) override;
		virtual std::shared_ptr< RHITexture2D> RHICreateTexture2D(const core::FLinearColor& Color) override;
		virtual std::shared_ptr< RHITexture2D> RHICreateHDRTexture2D(const std::wstring& FileName) override;
		virtual std::shared_ptr< RHITexture1D> RHICreateTexture1D(EPixelFormat Format, int32_t Flags, int32_t SizeX, void* InBuffer, int RowBytes) override;
		virtual std::shared_ptr< RHITextureCube> RHICreateTextureCube(EPixelFormat Format, int32_t SizeX, int32_t SizeY, uint32_t NumMips, bool CreateDepth) override;
		virtual std::shared_ptr< RHIUnorderedAccessView> RHICreateUnorderedAccessView(EPixelFormat Format, int32_t SizeX, int32_t SizeY) override;
		virtual std::shared_ptr< RHIUnorderedAccessView> RHICreateUnorderedAccessView(std::shared_ptr< RHITexture2D> Tex2D) override;

		virtual std::shared_ptr< RHIRenderTarget> RHICreateRenderTarget(EPixelFormat Format, int32_t SizeX, int32_t SizeY, uint32_t NumMips, bool IsMultiSampled, bool CreateDepth) override;

		virtual std::shared_ptr< RHIVertexShader> RHICreateVertexShader(const std::wstring& FileName, const std::string& VSMain, const RHIVertexDeclare& VertexDeclare , const std::vector<RHIShaderMacro>& MacroDefines) override;
		virtual std::shared_ptr< RHIPixelShader> RHICreatePixelShader(const std::wstring& FileName, const std::string& PSMain, const std::vector<RHIShaderMacro>& MacroDefines) override;
		virtual std::shared_ptr< RHIComputeShader> RHICreateComputeShader(const std::wstring& FileName, const std::string& CSMain, const std::vector<RHIShaderMacro>& MacroDefines) override;

		virtual std::shared_ptr< RHISamplerState> RHICreateSamplerState(const SamplerStateInitializerRHI& Initializer) override;
		virtual std::shared_ptr< RHIRasterizerState> RHICreateRasterizerState(const RasterizerStateInitializerRHI& Initializer) override;
		virtual std::shared_ptr< RHIBlendState> RHICreateBlendState(const BlendStateInitializerRHI& Initializer) override;
		virtual std::shared_ptr< RHIDepthStencilState> RHICreateDepthStencilState(const DepthStencilStateInitializerRHI& Initializer) override;
		virtual std::shared_ptr< RHITilePool> RHICreateTilePool(std::shared_ptr< RHITexture2D> Tex2D) override;
		

		// Accessors.
		ID3D11Device* GetDevice() const;
		ID3D11DeviceContext* GetDeviceContext() const;
		IDXGIFactory1* GetFactory() const;

		D3D11StateCacheBase& GetStateCache();
		void ClearState();

		ID3D11Device2* GetDevice2() const;
		ID3D11DeviceContext2* GetDeviceContext2() const;
		bool FindAdapter();

	private:
		bool InitD3DDevice();
		D3D11DynamicRHIPrivate* d_ptr = nullptr;
	};
}