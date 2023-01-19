#pragma once
#include "RHI/DynamicRHI.h"
#include "RHIPrivate/D3D11RHIDeclare.h"

namespace RenderCore
{
	struct D3D11DynamicRHIP;
	class D3D11StateCacheBase;

	class D3D11DynamicRHI : public DynamicRHI
	{
	public:
		D3D11DynamicRHI();
		virtual ~D3D11DynamicRHI();

		/** Initializes the RHI; separate from IDynamicRHIModule::CreateRHI so that GDynamicRHI is set when it is called. */
		virtual void Init() override;

		/** Shutdown the RHI; handle shutdown and resource destruction before the RHI's actual destructor is called (so that all resources of the RHI are still available for shutdown). */
		virtual void Shutdown() override;

		virtual const TCHAR* GetName() { return TEXT("D3D11"); }

		virtual std::shared_ptr< RHICommandContext> GetDefaultCommandContext() override;

		virtual std::shared_ptr< RHIViewPort> RHICreateViewport(void* WindowHandle, uint32_t SizeX, uint32_t SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat) override;
		virtual std::shared_ptr< RHIVertexBuffer> RHICreateVertexBuffer(const void* Data, EBufferUsageFlags InUsage, int32_t StrideByteWidth, int32_t Count) override;
		virtual void RHIUpdateVertexBuffer(std::shared_ptr< RHIVertexBuffer> VertexBuffer,const void* InData, int32_t nVertex, int32_t sizePerVertex) override;
		virtual std::shared_ptr< RHIIndexBuffer> RHICreateIndexBuffer(const uint16_t* InData, EBufferUsageFlags InUsage, int32_t IndexCount) override;
		virtual std::shared_ptr< RHIIndexBuffer> RHICreateIndexBuffer(const uint32_t* InData, EBufferUsageFlags InUsage, int32_t IndexCount) override;

		virtual std::shared_ptr< RHITexture2D> RHICreateTexture2D(EPixelFormat Format, ETextureCreateFlags Flags, int32_t SizeX, int32_t SizeY, void* InBuffer = nullptr, int RowBytes = 0) override;
		virtual std::shared_ptr< RHITexture2D> RHICreateTexture2D(const std::wstring& FileName) override;
		virtual std::shared_ptr< RHITexture2D> RHICreateTexture2D(const math::Vector4& Color) override;
		virtual std::shared_ptr< RHITexture2D> RHICreateHDRTexture2D(const std::wstring& FileName) override;

		virtual std::shared_ptr< RHITexture1D> RHICreateTexture1D(EPixelFormat Format, ETextureCreateFlags Flags, int32_t SizeX, void* InBuffer, int RowBytes) override;

		virtual std::shared_ptr< RHIRenderTarget> RHICreateRenderTarget(std::shared_ptr< RHITexture2D> Tex, bool CreateDepth) override;
		virtual std::shared_ptr< RHIRenderTarget> RHICreateRenderTarget(EPixelFormat Format, int32_t SizeX, int32_t SizeY, bool CreateDepth) override;

		virtual std::shared_ptr< RHIVertexShader> RHICreateVertexShader(const std::wstring& FileName, const std::string& VSMain, const RHIVertexDeclare& VertexDeclare , const std::vector<RHIShaderMacro>& MacroDefines) override;
		virtual std::shared_ptr< RHIPixelShader> RHICreatePixelShader(const std::wstring& FileName, const std::string& PSMain, const std::vector<RHIShaderMacro>& MacroDefines) override;

		// Accessors.
		ID3D11Device* GetDevice() const;
		ID3D11DeviceContext* GetDeviceContext() const;
		IDXGIFactory1* GetFactory() const;

		D3D11StateCacheBase& GetStateCache();

	private:
		bool InitD3DDevice();
		bool FindAdapter();

	private:
		std::shared_ptr< D3D11DynamicRHIP> Impl;
	};
}