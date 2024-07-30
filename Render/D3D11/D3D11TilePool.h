#pragma once
#include "RHI/RHITilePool.h"

namespace RenderCore
{
	struct D3D11TilePoolPrivate;
	class D3D11DynamicRHI;

	class D3D11TilePool : public RHITilePool
	{
	public:
		D3D11TilePool(D3D11DynamicRHI* D3D11RHI);
		virtual ~D3D11TilePool();

		bool CreatePool(std::shared_ptr< RHITexture2D> TexRHI) override;
		bool UpdateTileMappings(std::shared_ptr< RHITexture2D> TexRHI) override;
		void UpdateTiles(std::shared_ptr< RHITexture2D> TexRHI, std::shared_ptr<uint8_t> Data) override;
	private:
		D3D11TilePoolPrivate* d_ptr;
	};
}