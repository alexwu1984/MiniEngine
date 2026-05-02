#include "RHI/RHITexture1D.h"
#include "RHIPrivate/D3D11RHIDeclare.h"

namespace RenderCore
{
	class D3D11DynamicRHI;
	struct D3D11Texture1DP;

	class D3D11Texture1D : public RHITexture1D
	{
	public:
		D3D11Texture1D(D3D11DynamicRHI* D3D11RHI);
		virtual ~D3D11Texture1D() {}

		virtual bool CreateWithData(EPixelFormat Format, int32_t Flags, int32_t SizeX, void* InBuffer, int32_t RowBytes) override;
		virtual int32_t GetSize() const override;

		ID3D11Texture1D* GetNativeTex() const;
		ID3D11ShaderResourceView* GetSRV() const;

	private:
		std::shared_ptr< D3D11Texture1DP> Impl;
	};
}