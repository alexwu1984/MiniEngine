#include "D3D12/D3D12Texture2D.h"
#include "D3D12/D3D12Resource.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12CommandContext.h"
#include "DirectXTex/DirectXTex.h"
#define STBI_FAILURE_USERMSG
#include "tinygltf/stb_image.h"

namespace RenderCore
{
	static std::atomic_uint32_t gCounter = 0;
	struct D3D12Texture2DPrivate
	{
		EPixelFormat PixFormat = EPixelFormat::PF_Unknown;
		DXGI_FORMAT PlatformResourceFormat = DXGI_FORMAT_UNKNOWN;
		FD3D12Resource* Resource = nullptr;
		core::vec2i Size;
		uint32_t NumMipMaps = 1;
		int32_t InFlags = TexCreate_ShaderResource;
		D3D12_CPU_DESCRIPTOR_HANDLE DSV{ D3D12_GPU_VIRTUAL_ADDRESS_NULL };
		D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle{ D3D12_GPU_VIRTUAL_ADDRESS_NULL };
		D3D12_CPU_DESCRIPTOR_HANDLE SRVHandle{ D3D12_GPU_VIRTUAL_ADDRESS_NULL }, SRVHandleMips{ D3D12_GPU_VIRTUAL_ADDRESS_NULL };
		D3D12_CPU_DESCRIPTOR_HANDLE UAVHandle{ D3D12_GPU_VIRTUAL_ADDRESS_NULL }, UAVHandleMips{ D3D12_GPU_VIRTUAL_ADDRESS_NULL };

		~D3D12Texture2DPrivate()
		{
			if (Resource)
				Resource->Release();
		}
	};

	static std::shared_ptr<uint8_t> GetImageData(const std::wstring& path, int32_t& SizeX, int32_t& SizeY)
	{
		std::string Utf8FileName = core::ucs2_u8(path);
		int32_t ImageChannel = 0;
		std::shared_ptr<uint8_t> ImageBuffer(stbi_load(Utf8FileName.c_str(), &SizeX, &SizeY, &ImageChannel, 4), [](uint8_t* p) {stbi_image_free(p); });
		return ImageBuffer;
	}

	D3D12Texture2D::D3D12Texture2D(std::weak_ptr<FD3D12Adapter> InParentAdapter)
		:FD3D12AdapterChild(InParentAdapter), d_ptr(new D3D12Texture2DPrivate())
	{

	}

	D3D12Texture2D::~D3D12Texture2D()
	{
		delete d_ptr;
	}

	bool D3D12Texture2D::CreateTexture2D(EPixelFormat InFormat, int32_t InFlags, int32_t SizeX, int32_t SizeY, 
											 int32_t SizeZ /*= 1*/, uint32_t NumMips /*= 1*/, void* InBuffer /*= nullptr*/, int RowBytes /*= 0*/)
	{
		C_P(D3D12Texture2D);
		d->Size.cx = SizeX;
		d->Size.cy = SizeY;
		d->PixFormat = InFormat;
		d->PlatformResourceFormat = FindSharedResourceDXGIFormat((DXGI_FORMAT)GPixelFormats[InFormat].PlatformFormat,false);
		d->InFlags = InFlags;
		d->NumMipMaps = (NumMips == 0) ? ComputeNumMips(SizeX, SizeY) : NumMips;
		D3D12_RESOURCE_FLAGS Flags = CombineResourceFlags(InFlags);
		D3D12_RESOURCE_DESC ResDesc = DescribeTex2D(SizeX, SizeY, 1, d->NumMipMaps, d->PlatformResourceFormat, Flags);

		ResDesc.SampleDesc.Count = 1;
		ResDesc.SampleDesc.Quality = 0;

		CD3DX12_HEAP_PROPERTIES HeapProps(D3D12_HEAP_TYPE_DEFAULT);
		if (InFlags & TexCreate_CPUReadback)
		{
			HeapProps.Type = D3D12_HEAP_TYPE_READBACK;
		}
		else if (InFlags & TexCreate_CPUWritable)
		{
			HeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
		}

		const bool FillClearValue = ((ResDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)|| 
			                        (Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)||
									(Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL));

		D3D12_CLEAR_VALUE ClearValue = {};
		ClearValue.Format = d->PlatformResourceFormat;
		std::wstring Name = core::formatw("SizeX:", SizeX, "SizeY:", SizeY,++gCounter);
		HRESULT hr = GetParentAdapter()->CreateCommittedResource(ResDesc, HeapProps, D3D12_RESOURCE_STATE_COPY_DEST, FillClearValue ? &ClearValue : nullptr, &d->Resource, Name.c_str());
		if (FAILED(hr))
		{
			return false;
		}

		size_t RowPitch, SlicePitch;
		DirectX::ComputePitch(d->PlatformResourceFormat, SizeX, SizeY, RowPitch, SlicePitch);

		if (InBuffer)
		{
			D3D12_SUBRESOURCE_DATA TexData;
			TexData.pData = InBuffer;
			TexData.RowPitch = RowPitch;
			TexData.SlicePitch = SlicePitch;
			GetParentDevice()->GetDefaultCommandContext()->InitializeTexture(d->Resource, d->NumMipMaps, &TexData);
		}

		if (InFlags & TexCreate_DepthStencilTargetable)
		{
			CreateDerivedViewsForDepthRes(d->PlatformResourceFormat);
		}
		else
		{
			CreateDerivedViews(d->PlatformResourceFormat, d->NumMipMaps);
		}
		
		return d->SRVHandle.ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL;
	}

	bool D3D12Texture2D::CreateFromFile(const std::wstring& FileName)
	{
		int32_t SizeX = 0;
		int32_t SizeY = 0;
		std::shared_ptr<uint8_t> ImageBuffer = GetImageData(FileName, SizeX, SizeY);
		if (ImageBuffer)
		{
			return CreateTexture2D(PF_B8G8R8A8, TexCreate_ShaderResource, SizeX, SizeY, 1, 1, ImageBuffer.get());
		}
		return false;
	}

	bool D3D12Texture2D::CreateHDRFromFile(const std::wstring& FileName)
	{
		C_P(D3D12Texture2D);
		DirectX::ScratchImage Image;
		HRESULT hr = DirectX::LoadFromHDRFile(FileName.c_str(), nullptr, Image);
		if (FAILED(hr))
		{
			return false;
		}
		return CreateFromImage(Image,FileName);
	}

	core::vec2i D3D12Texture2D::GetSize() const
	{
		C_P(const D3D12Texture2D);
		return d->Size;
	}

	EPixelFormat D3D12Texture2D::GetPixelFormat() const
	{
		C_P(const D3D12Texture2D);
		return d->PixFormat;
	}

	void D3D12Texture2D::CreateFromSwapChain(const std::wstring& Name, ID3D12Resource* BaseResource)
	{
		Assert(BaseResource);
		C_P(D3D12Texture2D);
		D3D12_RESOURCE_DESC Desc = BaseResource->GetDesc();
		d->Size.cx = (int32_t)Desc.Width;
		d->Size.cy = (int32_t)Desc.Height;
		d->Resource = new FD3D12Resource(GetParentDevice(), BaseResource, D3D12_RESOURCE_STATE_PRESENT, Desc);
		d->Resource->SetName(Name.c_str());
		d->Resource->AddRef();

		d->RTVHandle = GetParentDevice()->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		GetParentDevice()->GetDevice()->CreateRenderTargetView(d->Resource->GetResource(), nullptr, d->RTVHandle);
	}

	const D3D12_CPU_DESCRIPTOR_HANDLE& D3D12Texture2D::GetSRV(void) const
	{
		C_P(const D3D12Texture2D);
		return d->SRVHandle;
	}

	const D3D12_CPU_DESCRIPTOR_HANDLE& D3D12Texture2D::GetRTV(void) const
	{
		C_P(const D3D12Texture2D);
		return d->RTVHandle;
	}

	const D3D12_CPU_DESCRIPTOR_HANDLE& D3D12Texture2D::GetDSV(void) const
	{
		C_P(const D3D12Texture2D);
		return d->DSV;
	}

	FD3D12Resource* D3D12Texture2D::GetResource() const
	{
		C_P(const D3D12Texture2D);
		return d->Resource;
	}

	std::shared_ptr<FD3D12Device> D3D12Texture2D::GetParentDevice() const
	{
		return GetParentAdapter()->GetDevice();
	}

	void D3D12Texture2D::CreateDerivedViews(DXGI_FORMAT Format, uint32_t NumMips)
	{
		std::shared_ptr<FD3D12Device> Device = GetParentDevice();
		Assert(Device.get());
		C_P(D3D12Texture2D);

		D3D12_RENDER_TARGET_VIEW_DESC RTVDesc = {};
		D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};

		RTVDesc.Format = Format;
		RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		RTVDesc.Texture2D.MipSlice = 0;

		d->SRVHandle = Device->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		SRVDesc.Format = Format;
		SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		SRVDesc.Texture2D.MipLevels = NumMips;
		SRVDesc.Texture2D.MostDetailedMip = 0;
		Device->GetDevice()->CreateShaderResourceView(d->Resource->GetResource(), &SRVDesc, d->SRVHandle);

		if (d->InFlags & TexCreate_UAV)
		{
			d->UAVHandle = Device->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			UAVDesc.Format = Format;
			UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			UAVDesc.Texture2D.MipSlice = 0;
			Device->GetDevice()->CreateUnorderedAccessView(d->Resource->GetResource(), nullptr, &UAVDesc, d->UAVHandle);
		}

		if (NumMips > 1)
		{
			d->SRVHandleMips = Device->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, NumMips);
			d->UAVHandleMips = Device->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, NumMips);

			uint32_t SRVUAVDescriptorSize = Device->GetDescriptorSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			D3D12_CPU_DESCRIPTOR_HANDLE CurrentSRVHandle = d->SRVHandleMips;
			for (uint32_t i = 0; i < NumMips; ++i)
			{
				SRVDesc.Texture2DArray.ArraySize = 1;
				SRVDesc.Texture2DArray.FirstArraySlice = 0;
				SRVDesc.Texture2DArray.MipLevels = 1;
				SRVDesc.Texture2DArray.MostDetailedMip = i;
				SRVDesc.Texture2DArray.PlaneSlice = 0;
				SRVDesc.Texture2DArray.ResourceMinLODClamp = 0.f;
				Device->GetDevice()->CreateShaderResourceView(d->Resource->GetResource(), &SRVDesc, CurrentSRVHandle);
				CurrentSRVHandle.ptr += SRVUAVDescriptorSize;
			}

			if (d->InFlags & TexCreate_UAV)
			{
				d->UAVHandleMips = Device->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, NumMips);
				D3D12_CPU_DESCRIPTOR_HANDLE CurrentUAVHandle = d->UAVHandleMips;
				for (uint32_t i = 0; i < NumMips; ++i)
				{
					UAVDesc.Texture2DArray.ArraySize = 1;
					UAVDesc.Texture2DArray.FirstArraySlice = 0;
					UAVDesc.Texture2DArray.MipSlice = i;
					UAVDesc.Texture2DArray.PlaneSlice = 0;
					Device->GetDevice()->CreateUnorderedAccessView(d->Resource->GetResource(), nullptr, &UAVDesc, CurrentUAVHandle);
					CurrentUAVHandle.ptr += SRVUAVDescriptorSize;
				}
			}
		}

		if (d->InFlags & TexCreate_RenderTargetable)
		{
			d->RTVHandle = Device->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			Device->GetDevice()->CreateRenderTargetView(d->Resource->GetResource(), &RTVDesc, d->RTVHandle);
		}
	}

	void D3D12Texture2D::CreateDerivedViewsForDepthRes(DXGI_FORMAT Format)
	{
		std::shared_ptr<FD3D12Device> Device = GetParentDevice();
		Assert(Device.get());
		C_P(D3D12Texture2D);

		ID3D12Resource* Resource = d->Resource->GetResource();

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
		dsvDesc.Format = Format;
		if (Resource->GetDesc().SampleDesc.Count == 1)
		{
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsvDesc.Texture2D.MipSlice = 0;
		}
		else
		{
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
		}

		d->DSV = Device->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		Device->GetDevice()->CreateDepthStencilView(Resource, &dsvDesc, d->DSV);

		d->SRVHandle = Device->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		// Create the shader resource view
		D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		SRVDesc.Format = FindDepthStencilDXGIFormat(Format);
		if (dsvDesc.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE2D)
		{
			SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			SRVDesc.Texture2D.MipLevels = 1;
		}
		else
		{
			SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
		}
		SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		Device->GetDevice()->CreateShaderResourceView(Resource, &SRVDesc, d->SRVHandle);
	}

	bool D3D12Texture2D::CreateFromImage(const DirectX::ScratchImage& Image, const std::wstring& Name)
	{
		C_P(D3D12Texture2D);
		d->Size.cx = (int32_t)Image.GetImages()->width;
		d->Size.cy = (int32_t)Image.GetImages()->height;
		const bool IsSRGB = true;

		std::shared_ptr<FD3D12Device> Device = GetParentDevice();
		win32::com_ptr<ID3D12Resource> Resoure;

		VERIFYD3DRESULT(DirectX::CreateTextureEx(Device->GetDevice(), Image.GetMetadata(), D3D12_RESOURCE_FLAG_NONE, IsSRGB, Resoure.get_init_ref()));
		if (!Resoure.is_valid())
		{
			return false;
		}

		for (int32_t index = 1; index < EPixelFormat::PF_MAX_COUT; ++index)
		{
			if (GPixelFormats[index].PlatformFormat == Image.GetMetadata().format)
			{
				d->PixFormat = static_cast<EPixelFormat>(index);
				break;
			}
		}

		d->PlatformResourceFormat = Image.GetMetadata().format;
		d->NumMipMaps = Image.GetMetadata().mipLevels;

		d->Resource = new FD3D12Resource(Device, Resoure.get(), D3D12_RESOURCE_STATE_COPY_DEST, Resoure->GetDesc());
		d->Resource->AddRef();
		d->Resource->SetName(Name.c_str());

		std::vector<D3D12_SUBRESOURCE_DATA> subresources;
		VERIFYD3DRESULT(PrepareUpload(Device->GetDevice(), Image.GetImages(), Image.GetImageCount(), Image.GetMetadata(), subresources));

		Assert(subresources.size() > 0);

		GetParentDevice()->GetDefaultCommandContext()->InitializeTexture(d->Resource, (UINT)subresources.size(), &subresources[0]);
		CreateDerivedViews(d->PlatformResourceFormat, d->NumMipMaps);

		return d->RTVHandle.ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL;
	}

}