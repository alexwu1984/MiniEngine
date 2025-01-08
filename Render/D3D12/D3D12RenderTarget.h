#pragma once
#include "RHI/RHIRenderTarget.h"
#include "D3D12/D3D12RHICommon.h"

namespace RenderCore
{
	struct D3D12RenderTargetPrivate;
	class FD3D12Resource;

	class D3D12RenderTarget : public RHIRenderTarget, public FD3D12AdapterChild
	{
	public:
		D3D12RenderTarget(std::weak_ptr<FD3D12Adapter> InParentAdapter);
		~D3D12RenderTarget();

		virtual bool Create(EPixelFormat Format, int32_t SizeX, int32_t SizeY, uint32_t NumMips, bool IsMultiSampled, bool CreateDepth) override;
		virtual core::vec2i GetSize() const override;
		virtual void Bind()  override;
		virtual void UnBind() override;
		virtual std::shared_ptr<RHITexture2D> GetTex() const override;

		const D3D12_CPU_DESCRIPTOR_HANDLE& GetSRV(void) const;
		const D3D12_CPU_DESCRIPTOR_HANDLE& GetRTV(void) const;
		const D3D12_CPU_DESCRIPTOR_HANDLE& GetDSV(void) const;
		const D3D12_CPU_DESCRIPTOR_HANDLE GetMipRTV(int Mip) const;
		FD3D12Resource* GetResource() const;
	private:
		D3D12RenderTargetPrivate* d_ptr = nullptr;
	};
};