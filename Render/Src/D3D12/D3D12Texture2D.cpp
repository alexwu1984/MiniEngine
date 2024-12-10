#include "D3D12/D3D12Texture2D.h"
#include "D3D12/D3D12Resource.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12WindowDevice.h"
#include "DirectXTex/DirectXTex.h"
#define STBI_FAILURE_USERMSG
#include "tinygltf/stb_image.h"


namespace RenderCore
{
	static std::atomic_uint32_t gCounter = 0;
	struct D3D12Texture2DPrivate
	{
		EPixelFormat PixFormat = PF_A8R8G8B8;
		FD3D12Resource* Resource = nullptr;
		core::vec2i Size;
		uint32_t NumMipMaps = 1;
		int32_t InFlags = 0;
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

		const DXGI_FORMAT PlatformResourceFormat = (DXGI_FORMAT)GPixelFormats[InFormat].PlatformFormat;

		d->NumMipMaps = (NumMips == 0) ? ComputeNumMips(SizeX, SizeX) : NumMips;
		d->InFlags = InFlags;
		D3D12_RESOURCE_FLAGS Flags = CombineResourceFlags(InFlags);
		D3D12_RESOURCE_DESC ResDesc = DescribeTex2D(SizeX, SizeX, 1, d->NumMipMaps, PlatformResourceFormat, Flags);

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

		D3D12_CLEAR_VALUE ClearValue = {};
		ClearValue.Format = PlatformResourceFormat;
		std::wstring Name = core::formatw("SizeX:", SizeX, "SizeY:", SizeY,++gCounter);
		HRESULT hr = GetParentAdapter()->CreateCommittedResource(ResDesc, HeapProps, D3D12_RESOURCE_STATE_COPY_DEST, &ClearValue, &d->Resource, Name.c_str());
		if (FAILED(hr))
		{
			return false;
		}
		size_t RowPitch = 0, SlicePitch = 0;
		DirectX::ComputePitch(PlatformResourceFormat, SizeX, SizeY, RowPitch, SlicePitch);
		if (InBuffer && RowPitch != RowBytes)
		{
			return false;
		}

		D3D12_SUBRESOURCE_DATA TexData;
		TexData.pData = InBuffer;
		TexData.RowPitch = RowPitch;
		TexData.SlicePitch = SlicePitch;

		CreateDerivedViews(PlatformResourceFormat, 1, d->NumMipMaps);
		return d->SRVHandle.ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL;
	}

	bool D3D12Texture2D::CreateFromFile(const std::wstring& FileName)
	{
		int32_t SizeX = 0;
		int32_t SizeY = 0;
		std::shared_ptr<uint8_t> ImageBuffer = GetImageData(FileName, SizeX, SizeY);
		if (ImageBuffer)
		{
			return CreateTexture2D(PF_B8G8R8A8, TexCreate_ShaderResource, SizeX, SizeY, 1, 1, ImageBuffer.get(), 4 * SizeX * sizeof(uint8_t));
		}
		return false;
	}

	bool D3D12Texture2D::CreateHDRFromFile(const std::wstring& FileName)
	{
		DirectX::ScratchImage image;
		HRESULT hr = DirectX::LoadFromHDRFile(FileName.c_str(), nullptr, image);

		//m_Width = (int)image.GetImages()->width;
		//m_Height = (int)image.GetImages()->height;

		//ThrowIfFailed(hr);
		//ID3D12Device* Device = D3D12RHI::Get().GetD3D12Device().Get();
		//ThrowIfFailed(DirectX::CreateTextureEx(Device, image.GetMetadata(), D3D12_RESOURCE_FLAG_NONE, IsSRGB, m_Resource.ReleaseAndGetAddressOf()));
		//InitializeState(D3D12_RESOURCE_STATE_COPY_DEST);

		//m_Resource->SetName(FileName.c_str());

		//std::vector<D3D12_SUBRESOURCE_DATA> subresources;
		//ThrowIfFailed(PrepareUpload(Device, image.GetImages(), image.GetImageCount(), image.GetMetadata(), subresources));

		//Assert(subresources.size() > 0);
		//FCommandContext::InitializeTexture(*this, (UINT)subresources.size(), &subresources[0]);

		//if (m_CpuDescriptorHandle.ptr == D3D12_CPU_VIRTUAL_ADDRESS_UNKNOWN)
		//	m_CpuDescriptorHandle = D3D12RHI::Get().AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		//D3D12RHI::Get().GetD3D12Device()->CreateShaderResourceView(m_Resource.Get(), nullptr, m_CpuDescriptorHandle);

		return false;
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

	const D3D12_CPU_DESCRIPTOR_HANDLE& D3D12Texture2D::GetRTV(void) const
	{
		C_P(const D3D12Texture2D);
		return d->RTVHandle;
	}

	FD3D12Resource* D3D12Texture2D::GetResource() const
	{
		C_P(const D3D12Texture2D);
		return d->Resource;
	}

	std::shared_ptr<FD3D12Device> D3D12Texture2D::GetParentDevice() const
	{
		return GetParentAdapter()->GetDevice(0);
	}

	void D3D12Texture2D::CreateDerivedViews(DXGI_FORMAT Format, uint32_t ArraySize, uint32_t NumMips /*= 1*/)
	{
		std::shared_ptr<FD3D12Device> Device = GetParentDevice();
		(ArraySize);
		Assert(Device.get());
		C_P(D3D12Texture2D);

		D3D12_RENDER_TARGET_VIEW_DESC RTVDesc = {};
		D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};

		RTVDesc.Format = Format;
		RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		RTVDesc.Texture2D.MipSlice = 0;

		if (d->SRVHandle.ptr == 0)
		{
			d->SRVHandle = Device->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}

		SRVDesc.Format = Format;
		SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		SRVDesc.Texture2D.MipLevels = NumMips;
		SRVDesc.Texture2D.MostDetailedMip = 0;
		Device->GetDevice()->CreateShaderResourceView(d->Resource->GetResource(), &SRVDesc, d->SRVHandle);

		if (d->InFlags & TexCreate_UAV)
		{
			if (d->UAVHandle.ptr == 0)
			{
				d->UAVHandle = Device->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}
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
			if (d->RTVHandle.ptr == 0)
			{
				d->RTVHandle = Device->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			}
			Device->GetDevice()->CreateRenderTargetView(d->Resource->GetResource(), &RTVDesc, d->RTVHandle);
		}
	}

}