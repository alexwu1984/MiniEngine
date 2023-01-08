#include "D3D11/D3D11RHI.h"
#include "RHIPrivate/D3D11RHIPrivate.h"
#include "D3D11/D3D11ViewPort.h"
#include "D3D11/D3D11ReourceTraits.h"

namespace RenderCore
{
	int64_t D3D11GlobalStats::GDedicatedVideoMemory{ 0 };
	int64_t D3D11GlobalStats::GDedicatedSystemMemory{ 0 };
	int64_t D3D11GlobalStats::GSharedSystemMemory{ 0 };
	int64_t D3D11GlobalStats::GTotalGraphicsMemory{ 0 };

	D3D11DynamicRHI::D3D11DynamicRHI()
		:Data(std::make_shared<D3D11DynamicRHIP>())
	{
		Data->CommandContext = std::make_shared<D3D11CommandContext>(this);
		// Initialize the platform pixel format map.
		GPixelFormats[PF_Unknown].PlatformFormat = DXGI_FORMAT_UNKNOWN;
		GPixelFormats[PF_A32B32G32R32F].PlatformFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
		GPixelFormats[PF_B8G8R8A8].PlatformFormat = DXGI_FORMAT_B8G8R8A8_TYPELESS;
		GPixelFormats[PF_G8].PlatformFormat = DXGI_FORMAT_R8_UNORM;
		GPixelFormats[PF_G16].PlatformFormat = DXGI_FORMAT_R16_UNORM;
		GPixelFormats[PF_DXT1].PlatformFormat = DXGI_FORMAT_BC1_TYPELESS;
		GPixelFormats[PF_DXT3].PlatformFormat = DXGI_FORMAT_BC2_TYPELESS;
		GPixelFormats[PF_DXT5].PlatformFormat = DXGI_FORMAT_BC3_TYPELESS;
		GPixelFormats[PF_BC4].PlatformFormat = DXGI_FORMAT_BC4_UNORM;
		GPixelFormats[PF_UYVY].PlatformFormat = DXGI_FORMAT_UNKNOWN;		// TODO: Not supported in D3D11
		GPixelFormats[PF_DepthStencil].PlatformFormat = DXGI_FORMAT_R24G8_TYPELESS;
		GPixelFormats[PF_DepthStencil].BlockBytes = 4;
		GPixelFormats[PF_X24_G8].PlatformFormat = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
		GPixelFormats[PF_X24_G8].BlockBytes = 4;
		GPixelFormats[PF_DepthStencil].Supported = true;
		GPixelFormats[PF_X24_G8].Supported = true;
		GPixelFormats[PF_ShadowDepth].PlatformFormat = DXGI_FORMAT_R16_TYPELESS;
		GPixelFormats[PF_ShadowDepth].BlockBytes = 2;
		GPixelFormats[PF_ShadowDepth].Supported = true;
		GPixelFormats[PF_R32_FLOAT].PlatformFormat = DXGI_FORMAT_R32_FLOAT;
		GPixelFormats[PF_G16R16].PlatformFormat = DXGI_FORMAT_R16G16_UNORM;
		GPixelFormats[PF_G16R16F].PlatformFormat = DXGI_FORMAT_R16G16_FLOAT;
		GPixelFormats[PF_G16R16F_FILTER].PlatformFormat = DXGI_FORMAT_R16G16_FLOAT;
		GPixelFormats[PF_G32R32F].PlatformFormat = DXGI_FORMAT_R32G32_FLOAT;
		GPixelFormats[PF_A2B10G10R10].PlatformFormat = DXGI_FORMAT_R10G10B10A2_UNORM;
		GPixelFormats[PF_A16B16G16R16].PlatformFormat = DXGI_FORMAT_R16G16B16A16_UNORM;
		GPixelFormats[PF_D24].PlatformFormat = DXGI_FORMAT_R24G8_TYPELESS;
		GPixelFormats[PF_R16F].PlatformFormat = DXGI_FORMAT_R16_FLOAT;
		GPixelFormats[PF_R16F_FILTER].PlatformFormat = DXGI_FORMAT_R16_FLOAT;

		GPixelFormats[PF_FloatRGB].PlatformFormat = DXGI_FORMAT_R11G11B10_FLOAT;
		GPixelFormats[PF_FloatRGB].BlockBytes = 4;
		GPixelFormats[PF_FloatRGBA].PlatformFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
		GPixelFormats[PF_FloatRGBA].BlockBytes = 8;

		GPixelFormats[PF_FloatR11G11B10].PlatformFormat = DXGI_FORMAT_R11G11B10_FLOAT;
		GPixelFormats[PF_FloatR11G11B10].BlockBytes = 4;
		GPixelFormats[PF_FloatR11G11B10].Supported = true;

		GPixelFormats[PF_V8U8].PlatformFormat = DXGI_FORMAT_R8G8_SNORM;
		GPixelFormats[PF_BC5].PlatformFormat = DXGI_FORMAT_BC5_UNORM;
		GPixelFormats[PF_A1].PlatformFormat = DXGI_FORMAT_R1_UNORM; // Not supported for rendering.
		GPixelFormats[PF_A8].PlatformFormat = DXGI_FORMAT_A8_UNORM;
		GPixelFormats[PF_R32_UINT].PlatformFormat = DXGI_FORMAT_R32_UINT;
		GPixelFormats[PF_R32_SINT].PlatformFormat = DXGI_FORMAT_R32_SINT;

		GPixelFormats[PF_R16_UINT].PlatformFormat = DXGI_FORMAT_R16_UINT;
		GPixelFormats[PF_R16_SINT].PlatformFormat = DXGI_FORMAT_R16_SINT;
		GPixelFormats[PF_R16G16B16A16_UINT].PlatformFormat = DXGI_FORMAT_R16G16B16A16_UINT;
		GPixelFormats[PF_R16G16B16A16_SINT].PlatformFormat = DXGI_FORMAT_R16G16B16A16_SINT;

		GPixelFormats[PF_R5G6B5_UNORM].PlatformFormat = DXGI_FORMAT_B5G6R5_UNORM;
		GPixelFormats[PF_R8G8B8A8].PlatformFormat = DXGI_FORMAT_R8G8B8A8_TYPELESS;
		GPixelFormats[PF_R8G8B8A8_UINT].PlatformFormat = DXGI_FORMAT_R8G8B8A8_UINT;
		GPixelFormats[PF_R8G8B8A8_SNORM].PlatformFormat = DXGI_FORMAT_R8G8B8A8_SNORM;
		GPixelFormats[PF_R8G8].PlatformFormat = DXGI_FORMAT_R8G8_UNORM;
		GPixelFormats[PF_R32G32B32A32_UINT].PlatformFormat = DXGI_FORMAT_R32G32B32A32_UINT;
		GPixelFormats[PF_R16G16_UINT].PlatformFormat = DXGI_FORMAT_R16G16_UINT;
		GPixelFormats[PF_R32G32_UINT].PlatformFormat = DXGI_FORMAT_R32G32_UINT;

		GPixelFormats[PF_BC6H].PlatformFormat = DXGI_FORMAT_BC6H_UF16;
		GPixelFormats[PF_BC7].PlatformFormat = DXGI_FORMAT_BC7_TYPELESS;
		GPixelFormats[PF_R8_UINT].PlatformFormat = DXGI_FORMAT_R8_UINT;
		GPixelFormats[PF_R8].PlatformFormat = DXGI_FORMAT_R8_UNORM;

		GPixelFormats[PF_R16G16B16A16_UNORM].PlatformFormat = DXGI_FORMAT_R16G16B16A16_UNORM;
		GPixelFormats[PF_R16G16B16A16_SNORM].PlatformFormat = DXGI_FORMAT_R16G16B16A16_SNORM;

		GPixelFormats[PF_NV12].PlatformFormat = DXGI_FORMAT_NV12;
		GPixelFormats[PF_NV12].Supported = true;
	}

	D3D11DynamicRHI::~D3D11DynamicRHI()
	{

	}

	void D3D11DynamicRHI::Init()
	{
		InitD3DDevice();
	}

	void D3D11DynamicRHI::Shutdown()
	{
		Data = {};
	}

	std::shared_ptr<RHICommandContext> D3D11DynamicRHI::GetDefaultCommandContext()
	{
		return Data->CommandContext;
	}

	std::shared_ptr<RHIViewPort> D3D11DynamicRHI::RHICreateViewport(void* WindowHandle, uint32_t SizeX, uint32_t SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat)
	{
		std::shared_ptr<D3D11ViewPort> ViewPortRHI = std::make_shared<D3D11ViewPort>(this, (HWND)WindowHandle,SizeX,SizeY);
		return ViewPortRHI;
	}

	std::shared_ptr<RHIVertexBuffer> D3D11DynamicRHI::RHICreateVertexBuffer(const void* Data, EBufferUsageFlags InUsage, int32_t StrideByteWidth, int32_t Count)
	{
		std::shared_ptr<D3D11VertexBuffer> VertexBufferRHI = std::make_shared<D3D11VertexBuffer>(this);
		if (VertexBufferRHI->CreateVertexBuffer(Data,InUsage,StrideByteWidth,Count))
		{
			return VertexBufferRHI;
		}
		else
		{
			return nullptr;
		}
	}

	void D3D11DynamicRHI::RHIUpdateVertexBuffer(std::shared_ptr< RHIVertexBuffer> VertexBuffer,const void* InData, int32_t nVertex, int32_t sizePerVertex)
	{
		VertexBuffer->UpdateVertexBUffer(InData, nVertex, sizePerVertex);
	}

	std::shared_ptr< RHIIndexBuffer> D3D11DynamicRHI::RHICreateIndexBuffer(const uint16_t* InData, EBufferUsageFlags InUsage, int32_t IndexCount)
	{
		std::shared_ptr<D3D11IndexBuffer> IndexBufferRHI = std::make_shared<D3D11IndexBuffer>(this);
		if (IndexBufferRHI->CreateIndexBuffer(InData,InUsage,IndexCount))
		{
			return IndexBufferRHI;
		}
		else
		{
			return nullptr;
		}
	}

	std::shared_ptr< RHIIndexBuffer> D3D11DynamicRHI::RHICreateIndexBuffer(const uint32_t* InData, EBufferUsageFlags InUsage, int32_t IndexCount)
	{
		std::shared_ptr<D3D11IndexBuffer> IndexBufferRHI = std::make_shared<D3D11IndexBuffer>(this);
		if (IndexBufferRHI->CreateIndexBuffer(InData, InUsage, IndexCount))
		{
			return IndexBufferRHI;
		}
		else
		{
			return nullptr;
		}
	}

	std::shared_ptr< RHITexture2D> D3D11DynamicRHI::RHICreateTexture2D(EPixelFormat Format, ETextureCreateFlags Flags, int32_t SizeX, int32_t SizeY, void* InBuffer /*= nullptr*/, int RowBytes /*= 0*/)
	{
		std::shared_ptr<D3D11Texture2D> Tex2DRHI = std::make_shared<D3D11Texture2D>(this);
		if (Tex2DRHI->CreateWithData(Format, Flags, SizeX, SizeY, InBuffer, RowBytes))
		{
			return Tex2DRHI;
		}
		else
		{
			return nullptr;
		}
	}

	std::shared_ptr< RHITexture2D> D3D11DynamicRHI::RHICreateTexture2D(const std::wstring& FileName)
	{
		std::shared_ptr<D3D11Texture2D> Tex2DRHI = std::make_shared<D3D11Texture2D>(this);
		if (Tex2DRHI->CreateFromFile(FileName))
		{
			return Tex2DRHI;
		}
		else
		{
			return nullptr;
		}
	}

	std::shared_ptr< RHITexture2D> D3D11DynamicRHI::RHICreateTexture2D(const math::Vector4& Color)
	{
		std::shared_ptr<D3D11Texture2D> Tex2DRHI = std::make_shared<D3D11Texture2D>(this);
		uint8_t tmp[] = { (uint8_t)(Color.r * 255),(uint8_t)(Color.g * 255),(uint8_t)(Color.b * 255),(uint8_t)(Color.a * 255) };
		if (Tex2DRHI->CreateWithData(EPixelFormat::PF_B8G8R8A8,ETextureCreateFlags::TexCreate_ShaderResource,1,1,tmp,4))
		{
			return Tex2DRHI;
		}
		else
		{
			return nullptr;
		}
	}

	std::shared_ptr< RHITexture2D> D3D11DynamicRHI::RHICreateHDRTexture2D(const std::wstring& FileName)
	{
		std::shared_ptr<D3D11Texture2D> Tex2DRHI = std::make_shared<D3D11Texture2D>(this);
		if (Tex2DRHI->CreateHDRFromFile(FileName))
		{
			return Tex2DRHI;
		}
		else
		{
			return nullptr;
		}
	}

	std::shared_ptr< RHITexture1D> D3D11DynamicRHI::RHICreateTexture1D(EPixelFormat Format, ETextureCreateFlags Flags, int32_t SizeX, void* InBuffer, int RowBytes)
	{
		std::shared_ptr<D3D11Texture1D> Tex1DRHI = std::make_shared<D3D11Texture1D>(this);
		if (Tex1DRHI->CreateWithData(Format,Flags,SizeX,InBuffer,RowBytes))
		{
			return Tex1DRHI;
		}
		else
		{
			return nullptr;
		}
	}

	std::shared_ptr< RHIRenderTarget> D3D11DynamicRHI::RHICreateRenderTarget(std::shared_ptr< RHITexture2D> Tex, bool CreateDepth)
	{
		std::shared_ptr<D3D11RenderTarget> RenderTargetRHI = std::make_shared<D3D11RenderTarget>(this);
		if (RenderTargetRHI->CreateWithTexture(Tex,CreateDepth))
		{
			return RenderTargetRHI;
		}
		else
		{
			return nullptr;
		}
	}

	std::shared_ptr< RHIRenderTarget> D3D11DynamicRHI::RHICreateRenderTarget(EPixelFormat Format, int32_t SizeX, int32_t SizeY, bool CreateDepth)
	{
		std::shared_ptr<D3D11RenderTarget> RenderTargetRHI = std::make_shared<D3D11RenderTarget>(this);
		if (RenderTargetRHI->Create(Format, SizeX,SizeY,CreateDepth))
		{
			return RenderTargetRHI;
		}
		else
		{
			return nullptr;
		}
	}

	ID3D11Device* D3D11DynamicRHI::GetDevice() const
	{
		return Data->Direct3DDevice.get();
	}

	ID3D11DeviceContext* D3D11DynamicRHI::GetDeviceContext() const
	{
		return Data->Direct3DDeviceIMContext.get();
	}

	IDXGIFactory1* D3D11DynamicRHI::GetFactory() const
	{
		return Data->DXGIFactory1.get();
	}

	D3D11StateCacheBase& D3D11DynamicRHI::GetStateCache()
	{
		return Data->StateCache;
	}

}

