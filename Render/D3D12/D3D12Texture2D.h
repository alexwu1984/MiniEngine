#pragma once
#include "RHI/RHITexture2D.h"
#include "D3D12/D3D12RHICommon.h"

namespace DirectX
{
	class ScratchImage;
}

namespace RenderCore
{
	class FD3D12Resource;
	struct D3D12Texture2DPrivate;

	class D3D12Texture2D : public RHITexture2D, public FD3D12AdapterChild
	{
	public:
		D3D12Texture2D(std::weak_ptr<FD3D12Adapter> InParentAdapter);
		virtual ~D3D12Texture2D();

		bool CreateTexture2D(EPixelFormat Format, int32_t Flags, int32_t SizeX, int32_t SizeY, int32_t SizeZ = 1,
			uint32_t NumMips = 1, void* InBuffer = nullptr, int RowBytes = 0) override;
		bool TryCreateTransientAliasingUAV(std::shared_ptr<class FD3D12TransientAliasingPool> Pool,
			EPixelFormat Format, int32_t SizeX, int32_t SizeY);
		bool CreateFromFile(const std::wstring& FileName) override;
		bool CreateHDRFromFile(const std::wstring& FileName) override;
		bool IsMultisampled() const override { return false; }
		core::vec2i GetSize()const override;
		uint32_t GetNumMips() const override;
		EPixelFormat GetPixelFormat() const override;
		DXGI_FORMAT GetPlatformResourceFormat() const;

		void CreateFromSwapChain(const std::wstring& Name, ID3D12Resource* BaseResource);
		const D3D12_CPU_DESCRIPTOR_HANDLE& GetSRV(void) const;
		const D3D12_CPU_DESCRIPTOR_HANDLE& GetUAV(void) const;
		const D3D12_CPU_DESCRIPTOR_HANDLE& GetRTV(void) const;
		const D3D12_CPU_DESCRIPTOR_HANDLE& GetDSV(void) const;
		const D3D12_CPU_DESCRIPTOR_HANDLE GetMipSRV(int Mip) const;
		const D3D12_CPU_DESCRIPTOR_HANDLE GetMipUAV(int Mip) const;
		const D3D12_CPU_DESCRIPTOR_HANDLE GetMipRTV(int Mip) const;
		FD3D12Resource* GetResource() const;
	private:
		std::shared_ptr<FD3D12Device> GetParentDevice() const;
		void CreateDerivedViews(DXGI_FORMAT Format, uint32_t NumMips);
		void CreateDerivedViewsForDepthRes(DXGI_FORMAT Format);
		bool CreateFromImage(const DirectX::ScratchImage& Image, const std::wstring& Name);
	private:
		D3D12Texture2DPrivate* d_ptr = nullptr;
	};
}