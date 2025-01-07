#pragma once
#include "RHI/RHITextureCube.h"
#include "D3D12/D3D12RHICommon.h"

namespace RenderCore
{
	struct D3D12TextureCubePrivate;
	class D3D12Texture2D;

	class D3D12TextureCube : public RHITextureCube, public FD3D12AdapterChild
	{
	public:
		D3D12TextureCube(std::weak_ptr<FD3D12Adapter> InParentAdapter);
		virtual ~D3D12TextureCube();

		virtual bool CreateTextureCube(EPixelFormat InFormat, int32_t SizeX, int32_t SizeY, uint32_t NumMips, bool CreateDepth) override;
		virtual core::vec2i GetSize() const override;
		virtual uint32_t GetNumMips() const override;

		D3D12_CPU_DESCRIPTOR_HANDLE GetRTV(int Face, int Mip) const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetCubeSRV(int Mip = -1) const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetFaceMipSRV(int Face, int Mip) const;

	private:
		std::shared_ptr<FD3D12Device> GetParentDevice() const;
		void CreateDerivedViews(DXGI_FORMAT Format, uint32_t ArraySize, uint32_t NumMips = 1);
		uint32_t GetSubresourceIndex(int Face, int Mip) const;
	private:
		D3D12TextureCubePrivate* d_ptr = nullptr;
	};
}