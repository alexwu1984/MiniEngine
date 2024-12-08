#include "D3D12/D3D12Texture2D.h"
#include "D3D12/D3D12Resource.h"
#include "D3D12/D3D12WindowDevice.h"

namespace RenderCore
{
	struct D3D12Texture2DPrivate
	{
		EPixelFormat PixFormat = PF_A8R8G8B8;
		FD3D12Resource* Resource = nullptr;
		core::vec2i Size;
		D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle{ D3D12_GPU_VIRTUAL_ADDRESS_NULL };

		~D3D12Texture2DPrivate()
		{
			if (Resource)
				Resource->Release();
		}
	};

	D3D12Texture2D::D3D12Texture2D(std::weak_ptr<FD3D12Device> InParentDevice)
		:FD3D12DeviceChild(InParentDevice), d_ptr(new D3D12Texture2DPrivate())
	{

	}

	D3D12Texture2D::~D3D12Texture2D()
	{
		delete d_ptr;
	}

	bool D3D12Texture2D::CreateD3D11Texture2D(EPixelFormat Format, int32_t Flags, int32_t SizeX, int32_t SizeY, 
											 int32_t SizeZ /*= 1*/, uint32_t NumMips /*= 1*/, void* InBuffer /*= nullptr*/, int RowBytes /*= 0*/)
	{
		return false;
	}

	bool D3D12Texture2D::CreateFromFile(const std::wstring& FileName)
	{
		return false;
	}

	bool D3D12Texture2D::CreateHDRFromFile(const std::wstring& FileName)
	{
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

}