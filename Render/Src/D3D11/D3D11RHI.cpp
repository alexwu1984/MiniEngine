#include "D3D11/D3D11RHI.h"
#include "RHIPrivate/D3D11RHIPrivate.h"
#include "RHI/RHI.h"
#include "D3D11/D3D11ViewPort.h"
#include "D3D11/D3D11ReourceTraits.h"
#include "math/math.h"
#include "core/timer.h"
#include "RHI/RHICachedStates.h"
#include "Imgui/imgui_impl_win32.h"
#include "Imgui/imgui_impl_dx11.h"
#include "common/crc.h"

namespace RenderCore
{
	int64_t D3D11GlobalStats::GDedicatedVideoMemory{ 0 };
	int64_t D3D11GlobalStats::GDedicatedSystemMemory{ 0 };
	int64_t D3D11GlobalStats::GSharedSystemMemory{ 0 };
	int64_t D3D11GlobalStats::GTotalGraphicsMemory{ 0 };

	D3D11DynamicRHI::D3D11DynamicRHI()
		:d_ptr(new D3D11DynamicRHIPrivate())
	{
		C_P(D3D11DynamicRHI);
		d->CommandContext = std::make_shared<D3D11CommandContext>(this);
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
		//DXGI_FORMAT_R32G8X24_TYPELESS
		GPixelFormats[PF_DepthStencil].PlatformFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		GPixelFormats[PF_DepthStencil].BlockBytes = 4;
		GPixelFormats[PF_X24_G8].PlatformFormat = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
		GPixelFormats[PF_X24_G8].BlockBytes = 4;
		GPixelFormats[PF_DepthStencil].Supported = true;
		GPixelFormats[PF_X24_G8].Supported = true;
		GPixelFormats[PF_ShadowDepth].PlatformFormat = DXGI_FORMAT_R32_TYPELESS;
		GPixelFormats[PF_ShadowDepth].BlockBytes = 4;
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
		uint32_t Seed1 = core::Cycles();
		uint32_t Seed2 = core::Cycles();

		math::RandInit(Seed1);
		math::SRandInit(Seed2);

		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

		InitD3DDevice();

		RHICachedStates::Initialize(this);
	}

	void D3D11DynamicRHI::Shutdown()
	{
		RHICachedStates::DestroyAll();
		if (d_ptr)
		{
			delete d_ptr;
			d_ptr = nullptr;
		}
		::ImGui_ImplDX11_Shutdown();
		::ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	std::shared_ptr<RHICommandContext> D3D11DynamicRHI::GetDefaultCommandContext()
	{
		C_P(D3D11DynamicRHI);
		return d->CommandContext;
	}

	std::shared_ptr<RHICommandContext> D3D11DynamicRHI::GetDefaultAsyncComputeContext()
	{
		C_P(D3D11DynamicRHI);
		return d->CommandContext;
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

	std::shared_ptr< RHIUniformBuffer> D3D11DynamicRHI::RHICreateUniformBuffer(uint32_t ConstantBufferSize)
	{
		return RHICreateUniformBuffer(nullptr, ConstantBufferSize);
	}

	std::shared_ptr< RHIUniformBuffer> D3D11DynamicRHI::RHICreateUniformBuffer(const void* Contents, uint32_t ConstantBufferSize)
	{
		std::shared_ptr<D3D11UniformBuffer> UniformBufferRHI = std::make_shared<D3D11UniformBuffer>(this);
		if (UniformBufferRHI->CreateUniformBuffer(Contents, ConstantBufferSize))
		{
			return UniformBufferRHI;
		}
		else
		{
			return nullptr;
		}
	}

	std::shared_ptr< RHITexture2D> D3D11DynamicRHI::RHICreateTexture2D(EPixelFormat Format, int32_t Flags, int32_t SizeX, int32_t SizeY, uint32_t NumMips, void* InBuffer /*= nullptr*/, int RowBytes /*= 0*/)
	{
		std::shared_ptr<D3D11Texture2D> Tex2DRHI = std::make_shared<D3D11Texture2D>(this);
		if (Tex2DRHI->CreateTexture2D(Format, Flags, SizeX, SizeY,1, NumMips, InBuffer, RowBytes))
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

	std::shared_ptr< RHITexture2D> D3D11DynamicRHI::RHICreateTexture2D(const core::FLinearColor& Color)
	{
		std::shared_ptr<D3D11Texture2D> Tex2DRHI = std::make_shared<D3D11Texture2D>(this);
		uint8_t tmp[] = { (uint8_t)(Color.R * 255),(uint8_t)(Color.G * 255),(uint8_t)(Color.B * 255),(uint8_t)(Color.A * 255) };
		if (Tex2DRHI->CreateTexture2D(EPixelFormat::PF_B8G8R8A8,ETextureCreateFlags::TexCreate_ShaderResource,1,1,1,1,tmp,4))
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

	std::shared_ptr< RHITexture1D> D3D11DynamicRHI::RHICreateTexture1D(EPixelFormat Format, int32_t Flags, int32_t SizeX, void* InBuffer, int RowBytes)
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

	std::shared_ptr<RHITextureCube> D3D11DynamicRHI::RHICreateTextureCube(EPixelFormat Format, int32_t SizeX, int32_t SizeY, uint32_t NumMips, bool CreateDepth)
	{
		std::shared_ptr<D3D11TextureCube> TexCubeRHI = std::make_shared<D3D11TextureCube>(this);
		if (TexCubeRHI->CreateTextureCube(Format,SizeX,SizeY,NumMips,CreateDepth))
		{
			return TexCubeRHI;
		}
		else
		{
			return nullptr;
		}
	}

	std::shared_ptr<RHIUnorderedAccessView> D3D11DynamicRHI::RHICreateUnorderedAccessView(EPixelFormat Format, int32_t SizeX, int32_t SizeY)
	{
		std::shared_ptr< RHITexture2D> Tex2D = RHICreateTexture2D(Format, ETextureCreateFlags::TexCreate_UAV|ETextureCreateFlags::TexCreate_ShaderResource, SizeX, SizeY,1);
		if (!Tex2D)
		{
			return nullptr;
		}
		return RHICreateUnorderedAccessView(Tex2D);
	}

	std::shared_ptr< RHIUnorderedAccessView> D3D11DynamicRHI::RHICreateUnorderedAccessView(std::shared_ptr< RHITexture2D> Tex2D)
	{
		std::shared_ptr<RHIUnorderedAccessView> UAV = std::make_shared<D3D11UnorderedAccessView>(this);
		if (UAV->CreateFromTexture(Tex2D, 0))
		{
			return UAV;
		}
		return nullptr;
	}

	std::shared_ptr< RHIRenderTarget> D3D11DynamicRHI::RHICreateRenderTarget(EPixelFormat Format, int32_t SizeX, int32_t SizeY, uint32_t NumMips, bool IsMultiSampled, bool CreateDepth)
	{
		std::shared_ptr<D3D11RenderTarget> RenderTargetRHI = std::make_shared<D3D11RenderTarget>(this);
		if (RenderTargetRHI->Create(Format, SizeX,SizeY, NumMips,IsMultiSampled, CreateDepth))
		{
			return RenderTargetRHI;
		}
		else
		{
			return nullptr;
		}
	}

	std::shared_ptr< RHIVertexShader> D3D11DynamicRHI::RHICreateVertexShader(const std::wstring& FileName, const std::string& VSMain, const RHIVertexDeclare& VertexDeclare, const std::vector<RHIShaderMacro>& MacroDefines /*= {}*/)
	{
		C_P(D3D11DynamicRHI);
		std::shared_ptr<D3D11VertexShader> VertexShaderRHI = std::make_shared<D3D11VertexShader>(this);

		auto Key = core::ucs2_u8(FileName) + VSMain;
		auto HashCode = core::Crc::MemCrc32(Key.c_str(), Key.length());
		HashCode = core::Crc::HashState(VertexDeclare.GetDeclareDesc().data(), VertexDeclare.GetDeclareDesc().size(),HashCode);
		auto It = d->ShaderCache.VertexShaderCache.find(HashCode);
		if (It != d->ShaderCache.VertexShaderCache.end())
		{
			return It->second;
		}

		if (VertexShaderRHI->CreateShader(FileName,VSMain,VertexDeclare,MacroDefines))
		{
			d->ShaderCache.VertexShaderCache.insert({ HashCode,VertexShaderRHI });
			return VertexShaderRHI;
		}
		else
		{
			return nullptr;
		}
	}

	std::shared_ptr< RHIPixelShader> D3D11DynamicRHI::RHICreatePixelShader(const std::wstring& FileName, const std::string& PSMain, const std::vector<RHIShaderMacro>& MacroDefines)
	{
		C_P(D3D11DynamicRHI);
		std::shared_ptr<D3D11PixelShader> PixelShaderRHI = std::make_shared<D3D11PixelShader>(this);
		
		size_t HashCode = core::HashString(core::ucs2_u8(FileName) + PSMain);
		auto It = d->ShaderCache.PixelShaderCache.find(HashCode);
		if (It != d->ShaderCache.PixelShaderCache.end())
		{
			return It->second;
		}

		if (PixelShaderRHI->CreateShader(FileName,PSMain, MacroDefines))
		{
			d->ShaderCache.PixelShaderCache.insert({ HashCode,PixelShaderRHI });
			return PixelShaderRHI;
		}
		else
		{
			return nullptr;
		}
	}

	std::shared_ptr< RHIComputeShader> D3D11DynamicRHI::RHICreateComputeShader(const std::wstring& FileName, const std::string& CSMain, const std::vector<RHIShaderMacro>& MacroDefines)
	{
		C_P(D3D11DynamicRHI);
		std::shared_ptr<D3D11ComputeShader> ComputeShaderRHI = std::make_shared<D3D11ComputeShader>(this);

		size_t HashCode = core::HashString(core::ucs2_u8(FileName) + CSMain);
		auto It = d->ShaderCache.ComputeShaderCache.find(HashCode);
		if (It != d->ShaderCache.ComputeShaderCache.end())
		{
			return It->second;
		}

		if (ComputeShaderRHI->CreateShader(FileName, CSMain, MacroDefines))
		{
			d->ShaderCache.ComputeShaderCache.insert({ HashCode,ComputeShaderRHI });
			return ComputeShaderRHI;
		}
		else
		{
			return nullptr;
		}
	}

	std::shared_ptr< RHISamplerState> D3D11DynamicRHI::RHICreateSamplerState(const SamplerStateInitializerRHI& Initializer)
	{
		std::shared_ptr<D3D11SamplerState> SampleStateRHI = std::make_shared<D3D11SamplerState>(this);
		if (SampleStateRHI->CreateSamplerState(Initializer))
		{
			return SampleStateRHI;
		}
		else
		{
			return nullptr;
		}
	}

	std::shared_ptr< RHIRasterizerState> D3D11DynamicRHI::RHICreateRasterizerState(const RasterizerStateInitializerRHI& Initializer)
	{
		std::shared_ptr<RHIRasterizerState> RasterizerStateRHI = std::make_shared<D3D11RasterizerState>(this);
		if (RasterizerStateRHI->CreateRasterizerState(Initializer))
		{
			return RasterizerStateRHI;
		}
		else
		{
			return nullptr;
		}
	}

	std::shared_ptr< RHIBlendState> D3D11DynamicRHI::RHICreateBlendState(const BlendStateInitializerRHI& Initializer)
	{
		std::shared_ptr<RHIBlendState> BlendStateRHI = std::make_shared<D3D11BlendState>(this);
		if (BlendStateRHI->CreateBlendState(Initializer))
		{
			return BlendStateRHI;
		}
		else
		{
			return nullptr;
		}
	}

	std::shared_ptr< RHIDepthStencilState> D3D11DynamicRHI::RHICreateDepthStencilState(const DepthStencilStateInitializerRHI& Initializer)
	{
		std::shared_ptr<RHIDepthStencilState> DepthStencilStateRHI = std::make_shared<D3D11DepthStencilState>(this);
		if (DepthStencilStateRHI->CreateDepthStencilState(Initializer))
		{
			return DepthStencilStateRHI;
		}
		else
		{
			return nullptr;
		}
	}


	std::shared_ptr< RHITilePool> D3D11DynamicRHI::RHICreateTilePool(std::shared_ptr< RHITexture2D> Tex2D)
	{
		std::shared_ptr<RHITilePool> TilePoolRHI = std::make_shared<D3D11TilePool>(this);
		if (TilePoolRHI->CreatePool(Tex2D))
		{
			return TilePoolRHI;
		}
		else
		{
			return nullptr;
		}
	}

	ID3D11Device* D3D11DynamicRHI::GetDevice() const
	{
		C_P(const D3D11DynamicRHI);
		return d->Direct3DDevice.get();
	}

	ID3D11DeviceContext* D3D11DynamicRHI::GetDeviceContext() const
	{
		C_P(const D3D11DynamicRHI);
		return d->Direct3DDeviceIMContext.get();
	}

	IDXGIFactory1* D3D11DynamicRHI::GetFactory() const
	{
		C_P(const D3D11DynamicRHI);
		return d->DXGIFactory1.get();
	}

	D3D11StateCacheBase& D3D11DynamicRHI::GetStateCache()
	{
		C_P(D3D11DynamicRHI);
		return d->StateCache;
	}


	void D3D11DynamicRHI::ClearState()
	{
		C_P(D3D11DynamicRHI);
		d->StateCache.ClearState();
	}


	ID3D11Device2* D3D11DynamicRHI::GetDevice2() const
	{
		C_P(const D3D11DynamicRHI);
		return d->Direct3DDevice2.get();
	}


	ID3D11DeviceContext2* D3D11DynamicRHI::GetDeviceContext2() const
	{
		C_P(const D3D11DynamicRHI);
		return d->Direct3DDeviceIMContext2.get();
	}

	bool D3D11DynamicRHIModule::IsSupported()
	{
		if (!_dynamicRHI)
			CreateRHI();
		return _dynamicRHI->FindAdapter();
	}

	std::shared_ptr<DynamicRHI> D3D11DynamicRHIModule::CreateRHI()
	{
		if (_dynamicRHI)
			return _dynamicRHI;
		_dynamicRHI = std::make_shared<D3D11DynamicRHI>();
		return _dynamicRHI;
	}

}
