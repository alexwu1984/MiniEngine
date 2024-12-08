#pragma once
#include "RHI/RHITexture2D.h"
#include "D3D12/D3D12CommandList.h"

namespace RenderCore
{
	struct D3D12Texture2DPrivate;

	class D3D12Texture2D : public RHITexture2D, public FD3D12DeviceChild
	{
	public:
		D3D12Texture2D(std::weak_ptr<FD3D12Device> ParentDevice);
		virtual ~D3D12Texture2D();

		bool CreateD3D11Texture2D(EPixelFormat Format, int32_t Flags, int32_t SizeX, int32_t SizeY, int32_t SizeZ = 1,
			uint32_t NumMips = 1, void* InBuffer = nullptr, int RowBytes = 0) override;
		bool CreateFromFile(const std::wstring& FileName) override;
		bool CreateHDRFromFile(const std::wstring& FileName) override;
		bool IsMultisampled() const override { return false; }
		core::vec2i GetSize()const override;
		virtual uint32_t GetNumMips() const { return 0; }
		virtual EPixelFormat GetPixelFormat() const;

		void CreateFromSwapChain(const std::wstring& Name, ID3D12Resource* BaseResource);
		const D3D12_CPU_DESCRIPTOR_HANDLE& GetRTV(void) const;
		FD3D12Resource* GetResource() const;

	private:
		D3D12Texture2DPrivate* d_ptr = nullptr;
	};
}