#include "D3D12/D3D12TextureCube.h"
#include "D3D12/D3D12Texture2D.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12Resource.h"

namespace RenderCore
{
	struct D3D12TextureCubePrivate
	{
		std::shared_ptr<D3D12Texture2D> DepthTex;

		EPixelFormat PixFormat = EPixelFormat::PF_Unknown;
		DXGI_FORMAT PlatformResourceFormat = DXGI_FORMAT_UNKNOWN;
		FD3D12Resource* Resource = nullptr;
		core::vec2i Size;
		uint32_t NumMipMaps = 1;
		int32_t InFlags = TexCreate_ShaderResource;
		D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle{ D3D12_GPU_VIRTUAL_ADDRESS_NULL };
		D3D12_CPU_DESCRIPTOR_HANDLE CubeSRVHandle{ D3D12_GPU_VIRTUAL_ADDRESS_NULL }, FaceMipSRVHandle{ D3D12_GPU_VIRTUAL_ADDRESS_NULL };

		~D3D12TextureCubePrivate()
		{
			if (Resource)
				Resource->Release();
		}
	};


	D3D12TextureCube::D3D12TextureCube(std::weak_ptr<FD3D12Adapter> InParentAdapter)
		:FD3D12AdapterChild(InParentAdapter)
		,d_ptr(new D3D12TextureCubePrivate())
	{

	}

	D3D12TextureCube::~D3D12TextureCube()
	{
		delete d_ptr;
	}

	bool D3D12TextureCube::CreateTextureCube(EPixelFormat InFormat, int32_t SizeX, int32_t SizeY, uint32_t NumMips, bool CreateDepth)
	{
		C_P(D3D12TextureCube);
		d->Size.cx = SizeX;
		d->Size.cy = SizeY;
		d->PixFormat = InFormat;
		d->InFlags |= TexCreate_RenderTargetable;
		d->PlatformResourceFormat = FindSharedResourceDXGIFormat((DXGI_FORMAT)GPixelFormats[InFormat].PlatformFormat, false);
		d->NumMipMaps = (NumMips == 0) ? ComputeNumMips(SizeX, SizeY) : NumMips;
		D3D12_RESOURCE_FLAGS Flags = CombineResourceFlags(d->InFlags);
		D3D12_RESOURCE_DESC ResDesc = DescribeTex2D(SizeX, SizeY, 6, d->NumMipMaps, d->PlatformResourceFormat, Flags);

		ResDesc.SampleDesc.Count = 1;
		ResDesc.SampleDesc.Quality = 0;

		CD3DX12_HEAP_PROPERTIES HeapProps(D3D12_HEAP_TYPE_DEFAULT);

		const bool FillClearValue = ((ResDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) ||
			(Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) ||
			(Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL));

		static uint32_t gCounter = 0;
		D3D12_CLEAR_VALUE ClearValue = {};
		ClearValue.Format = d->PlatformResourceFormat;
		std::wstring Name = core::formatw("W:", SizeX, "_H:", SizeY, "_Cube_", ++gCounter);
		HRESULT hr = GetParentAdapter()->CreateCommittedResource(ResDesc, HeapProps, D3D12_RESOURCE_STATE_COMMON, FillClearValue ? &ClearValue : nullptr, &d->Resource, Name.c_str());
		if (FAILED(hr))
		{
			return false;
		}
		
		if (CreateDepth)
		{
			if (!d->DepthTex->CreateTexture2D(RenderCore::PF_DepthStencil, ETextureCreateFlags::TexCreate_DepthStencilTargetable, SizeX, SizeY))
				return false;
		}

		CreateDerivedViews(d->PlatformResourceFormat, 6, d->NumMipMaps);
		return true;
	}

	core::vec2i D3D12TextureCube::GetSize() const
	{
		C_P(const D3D12TextureCube);
		return d->Size;
	}

	uint32_t D3D12TextureCube::GetNumMips() const
	{
		C_P(const D3D12TextureCube);
		return d->NumMipMaps;
	}

	DXGI_FORMAT D3D12TextureCube::GetPlatformResourceFormat() const
	{
		C_P(const D3D12TextureCube);
		return d->PlatformResourceFormat;
	}

	FD3D12Resource* D3D12TextureCube::GetResource() const
	{
		C_P(const D3D12TextureCube);
		return d->Resource;
	}

	FD3D12Resource* D3D12TextureCube::GetDepthResource() const
	{
		C_P(const D3D12TextureCube);
		if (!d->DepthTex)
			return nullptr;
		return d->DepthTex->GetResource();
	}

	D3D12_CPU_DESCRIPTOR_HANDLE D3D12TextureCube::GetRTV(int Face, int Mip) const
	{
		C_P(D3D12TextureCube);
		std::shared_ptr<FD3D12Device> Device = GetParentDevice();
		Assert(Device.get());
		uint32_t RTVDescriptorSize = Device->GetDescriptorSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_CPU_DESCRIPTOR_HANDLE Result = d->RTVHandle;
		Result.ptr += RTVDescriptorSize * GetSubresourceIndex(Face, Mip);
		return Result;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE D3D12TextureCube::GetCubeSRV(int Mip /*= -1*/) const
	{
		C_P(D3D12TextureCube);
		std::shared_ptr<FD3D12Device> Device = GetParentDevice();
		Assert(Device.get());
		uint32_t SRVDescriptorSize = Device->GetDescriptorSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		D3D12_CPU_DESCRIPTOR_HANDLE Result = d->CubeSRVHandle;
		Result.ptr += SRVDescriptorSize * (Mip + 1);
		return Result;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE D3D12TextureCube::GetFaceMipSRV(int Face, int Mip) const
	{
		C_P(D3D12TextureCube);
		std::shared_ptr<FD3D12Device> Device = GetParentDevice();
		Assert(Device.get());
		uint32_t SRVDescriptorSize = Device->GetDescriptorSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		D3D12_CPU_DESCRIPTOR_HANDLE Result = d->FaceMipSRVHandle;
		Result.ptr += SRVDescriptorSize * GetSubresourceIndex(Face, Mip);
		return Result;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE D3D12TextureCube::GetDSV(void) const
	{
		C_P(const D3D12TextureCube);
		if (!d->DepthTex)
			return { D3D12_GPU_VIRTUAL_ADDRESS_NULL };
		return d->DepthTex->GetDSV();
	}

	std::shared_ptr<FD3D12Device> D3D12TextureCube::GetParentDevice() const
	{
		return GetParentAdapter()->GetDevice();
	}

	void D3D12TextureCube::CreateDerivedViews(DXGI_FORMAT Format, uint32_t ArraySize, uint32_t NumMips /*= 1*/)
	{
		std::shared_ptr<FD3D12Device> Device = GetParentDevice();
		Assert(Device.get());
		C_P(D3D12TextureCube);
		uint32_t RTVDescriptorSize = Device->GetDescriptorSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		uint32_t SRVDescriptorSize = Device->GetDescriptorSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		Assert(ArraySize == 6);
		D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		SRVDesc.Format = Format;
		SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		SRVDesc.TextureCube.MipLevels = NumMips;
		SRVDesc.TextureCube.MostDetailedMip = 0;
		SRVDesc.TextureCube.ResourceMinLODClamp = 0.0f;

		d->CubeSRVHandle = Device->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1 + NumMips);

		D3D12_CPU_DESCRIPTOR_HANDLE CurCubeSRVHandle = d->CubeSRVHandle;
		Device->GetDevice()->CreateShaderResourceView(d->Resource->GetResource(), &SRVDesc, CurCubeSRVHandle);
		CurCubeSRVHandle.ptr += SRVDescriptorSize;
		for (uint32_t Mip = 0; Mip < NumMips; ++Mip)
		{
			SRVDesc.TextureCube.MipLevels = 1;
			SRVDesc.TextureCube.MostDetailedMip = Mip;
			Device->GetDevice()->CreateShaderResourceView(d->Resource->GetResource(), &SRVDesc, CurCubeSRVHandle);
			CurCubeSRVHandle.ptr += SRVDescriptorSize;
		}

		D3D12_RENDER_TARGET_VIEW_DESC RTVDesc = {};
		RTVDesc.Format = Format;
		RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		RTVDesc.Texture2DArray.PlaneSlice = 0;

		d->RTVHandle = Device->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, ArraySize * NumMips);
		d->FaceMipSRVHandle = Device->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, ArraySize * NumMips);

		D3D12_CPU_DESCRIPTOR_HANDLE CurrentRTVHandle = d->RTVHandle;
		D3D12_CPU_DESCRIPTOR_HANDLE CurrentSRVHandle = d->FaceMipSRVHandle;
		for (uint32_t Face = 0; Face < ArraySize; ++Face)
		{
			for (uint32_t Mip = 0; Mip < NumMips; ++Mip)
			{
				RTVDesc.Texture2DArray.MipSlice = Mip;
				RTVDesc.Texture2DArray.FirstArraySlice = Face;
				RTVDesc.Texture2DArray.ArraySize = 1;
				Device->GetDevice()->CreateRenderTargetView(d->Resource->GetResource(), &RTVDesc, CurrentRTVHandle);
				CurrentRTVHandle.ptr += RTVDescriptorSize;

				SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
				SRVDesc.Texture2DArray.MostDetailedMip = Mip;
				SRVDesc.Texture2DArray.MipLevels = 1;
				SRVDesc.Texture2DArray.FirstArraySlice = Face;
				SRVDesc.Texture2DArray.ArraySize = 1;
				SRVDesc.Texture2DArray.PlaneSlice = 0;
				SRVDesc.Texture2DArray.ResourceMinLODClamp = 0.f;
				Device->GetDevice()->CreateShaderResourceView(d->Resource->GetResource(), &SRVDesc, CurrentSRVHandle);
				CurrentSRVHandle.ptr += SRVDescriptorSize;
			}
		}
	}

	uint32_t D3D12TextureCube::GetSubresourceIndex(int Face, int Mip) const
	{
		C_P(D3D12TextureCube);
		return Face * d->NumMipMaps + Mip;
	}

}