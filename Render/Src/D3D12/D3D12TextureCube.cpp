#include "D3D12/D3D12TextureCube.h"
#include "D3D12/D3D12Texture2D.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12Resource.h"
#include "D3D12/D3D12Util.h"
#include "RHI/RHIDefinitions.h"

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
		FD3D12ResourceAllocator::FDescriptorAllocation RtvAlloc{};
		FD3D12ResourceAllocator::FDescriptorAllocation CubeSrvAlloc{};
		FD3D12ResourceAllocator::FDescriptorAllocation FaceMipSrvAlloc{};
		FD3D12ResourceAllocator::FDescriptorAllocation DsvFaceAlloc{};

		D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle{ D3D12_GPU_VIRTUAL_ADDRESS_NULL };
		D3D12_CPU_DESCRIPTOR_HANDLE CubeSRVHandle{ D3D12_GPU_VIRTUAL_ADDRESS_NULL }, FaceMipSRVHandle{ D3D12_GPU_VIRTUAL_ADDRESS_NULL };
		uint32_t DsvDescriptorSize = 0;
		bool bShadowDepthCube = false;

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
		// Return CPU descriptor ranges to the allocator to prevent unbounded growth.
		if (d_ptr)
		{
			std::shared_ptr<FD3D12Device> Device = GetParentDevice();
			if (Device)
			{
				Device->FreeDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, d_ptr->DsvFaceAlloc);
				Device->FreeDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, d_ptr->RtvAlloc);
				Device->FreeDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, d_ptr->CubeSrvAlloc);
				Device->FreeDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, d_ptr->FaceMipSrvAlloc);
			}
		}
		delete d_ptr;
	}

	bool D3D12TextureCube::CreateTextureCube(EPixelFormat InFormat, int32_t SizeX, int32_t SizeY, uint32_t NumMips, bool CreateDepth)
	{
		C_P(D3D12TextureCube);
		std::shared_ptr<FD3D12Device> Device = GetParentDevice();
		if (!Device)
			return false;

		if (InFormat == EPixelFormat::PF_ShadowDepth)
		{
			d->bShadowDepthCube = true;
			d->PixFormat = InFormat;
			d->Size.cx = SizeX;
			d->Size.cy = SizeY;
			d->NumMipMaps = (NumMips == 0) ? 1u : NumMips;
			d->InFlags = TexCreate_DepthStencilTargetable | TexCreate_ShaderResource;
			d->PlatformResourceFormat = DXGI_FORMAT_R32_TYPELESS;
			D3D12_RESOURCE_FLAGS Flags = CombineResourceFlags(d->InFlags);
			D3D12_RESOURCE_DESC ResDesc = DescribeTex2D(SizeX, SizeY, 6, d->NumMipMaps, d->PlatformResourceFormat, Flags);
			ResDesc.SampleDesc.Count = 1;
			ResDesc.SampleDesc.Quality = 0;

			CD3DX12_HEAP_PROPERTIES HeapProps(D3D12_HEAP_TYPE_DEFAULT);
			D3D12_CLEAR_VALUE ClearValue = {};
			ClearValue.Format = DXGI_FORMAT_D32_FLOAT;
			ClearValue.DepthStencil.Depth = 1.0f;
			ClearValue.DepthStencil.Stencil = 0;

			static uint32_t gShadowCubeCounter = 0;
			std::wstring Name = core::formatw("W:", SizeX, "_H:", SizeY, "_ShadowCube_", ++gShadowCubeCounter);
			HRESULT hr = GetParentAdapter()->CreateCommittedResource(ResDesc, HeapProps, D3D12_RESOURCE_STATE_COMMON, &ClearValue, &d->Resource, Name.c_str());
			if (FAILED(hr))
				return false;

			ID3D12Device* D3DDevice = Device->GetDevice();
			Assert(D3DDevice);
			d->DsvDescriptorSize = Device->GetDescriptorSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
			d->DsvFaceAlloc = Device->AllocateDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 6);
			if (!d->DsvFaceAlloc.IsValid())
				return false;

			D3D12_CPU_DESCRIPTOR_HANDLE CurDsv = d->DsvFaceAlloc.Cpu;
			for (uint32_t face = 0; face < 6u; ++face)
			{
				D3D12_DEPTH_STENCIL_VIEW_DESC DsvDesc = {};
				DsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
				DsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
				DsvDesc.Flags = D3D12_DSV_FLAG_NONE;
				DsvDesc.Texture2DArray.MipSlice = 0;
				DsvDesc.Texture2DArray.FirstArraySlice = face;
				DsvDesc.Texture2DArray.ArraySize = 1;
				D3DDevice->CreateDepthStencilView(d->Resource->GetResource(), &DsvDesc, CurDsv);
				CurDsv.ptr += d->DsvDescriptorSize;
			}

			d->CubeSrvAlloc = Device->AllocateDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
			if (!d->CubeSrvAlloc.IsValid())
				return false;
			d->CubeSRVHandle = d->CubeSrvAlloc.Cpu;
			D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
			SrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
			SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			SrvDesc.TextureCube.MostDetailedMip = 0;
			SrvDesc.TextureCube.MipLevels = d->NumMipMaps;
			SrvDesc.TextureCube.ResourceMinLODClamp = 0.f;
			D3DDevice->CreateShaderResourceView(d->Resource->GetResource(), &SrvDesc, d->CubeSRVHandle);

			return true;
		}

		d->bShadowDepthCube = false;
		d->DsvDescriptorSize = 0;
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
			if (!d->DepthTex)
				d->DepthTex = std::make_shared<D3D12Texture2D>(GetParentAdapter());
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

	bool D3D12TextureCube::IsShadowDepthCube() const
	{
		C_P(const D3D12TextureCube);
		return d->bShadowDepthCube;
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
		if (d->bShadowDepthCube)
			return d->Resource;
		if (!d->DepthTex)
			return nullptr;
		return d->DepthTex->GetResource();
	}

	D3D12_CPU_DESCRIPTOR_HANDLE D3D12TextureCube::GetRTV(int Face, int Mip) const
	{
		C_P(D3D12TextureCube);
		if (d->bShadowDepthCube)
			return { D3D12_GPU_VIRTUAL_ADDRESS_NULL };
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
		if (d->bShadowDepthCube)
			return d->CubeSRVHandle;
		if (Mip < 0)
		{
			return d->CubeSRVHandle; // -1 means whole mipmap chain
		}
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
		if (d->bShadowDepthCube)
			return { D3D12_GPU_VIRTUAL_ADDRESS_NULL };
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
		if (d->bShadowDepthCube)
			return d->DsvFaceAlloc.IsValid() ? d->DsvFaceAlloc.Cpu : D3D12_CPU_DESCRIPTOR_HANDLE{ D3D12_GPU_VIRTUAL_ADDRESS_NULL };
		if (!d->DepthTex)
			return { D3D12_GPU_VIRTUAL_ADDRESS_NULL };
		return d->DepthTex->GetDSV();
	}

	D3D12_CPU_DESCRIPTOR_HANDLE D3D12TextureCube::GetFaceDSV(int FaceIndex) const
	{
		C_P(const D3D12TextureCube);
		if (!d->bShadowDepthCube || !d->DsvFaceAlloc.IsValid() || FaceIndex < 0 || FaceIndex >= 6 || d->DsvDescriptorSize == 0)
			return { D3D12_GPU_VIRTUAL_ADDRESS_NULL };
		D3D12_CPU_DESCRIPTOR_HANDLE h = d->DsvFaceAlloc.Cpu;
		h.ptr += static_cast<uint64_t>(d->DsvDescriptorSize) * static_cast<uint32_t>(FaceIndex);
		return h;
	}

	std::shared_ptr<FD3D12Device> D3D12TextureCube::GetParentDevice() const
	{
		std::shared_ptr<FD3D12Adapter> Adapter = TryGetParentAdapter();
		if (!Adapter)
			return {};
		return Adapter->GetDevice();
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

		d->CubeSrvAlloc = Device->AllocateDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1 + NumMips);
		d->CubeSRVHandle = d->CubeSrvAlloc.Cpu;

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

		d->RtvAlloc = Device->AllocateDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, ArraySize * NumMips);
		d->RTVHandle = d->RtvAlloc.Cpu;
		d->FaceMipSrvAlloc = Device->AllocateDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, ArraySize * NumMips);
		d->FaceMipSRVHandle = d->FaceMipSrvAlloc.Cpu;

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