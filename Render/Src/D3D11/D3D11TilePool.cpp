#include "D3D11/D3D11TilePool.h"
#include "RHIPrivate/D3D11RHIPrivate.h"
#include "D3D11/D3D11RHI.h"

namespace RenderCore
{
	struct D3D11TilePoolPrivate
	{
		D3D11DynamicRHI* D3D11RHI{ nullptr };
		win32::com_ptr<ID3D11Buffer> TiledPool;
	};

	static const UINT TileSizeInBytes = 0x10000;

	D3D11TilePool::D3D11TilePool(D3D11DynamicRHI* D3D11RHI)
		:d_ptr(new D3D11TilePoolPrivate())
	{
		C_P(D3D11TilePool);
		d->D3D11RHI = D3D11RHI;
	}

	D3D11TilePool::~D3D11TilePool()
	{
		delete d_ptr;
	}

	bool D3D11TilePool::CreatePool(uint32_t PoolSizeInTiles)
	{
		C_P(D3D11TilePool);
		// Create the tile pool.
		D3D11_BUFFER_DESC tilePoolDesc;
		ZeroMemory(&tilePoolDesc, sizeof(tilePoolDesc));
		tilePoolDesc.ByteWidth = TileSizeInBytes * PoolSizeInTiles;
		tilePoolDesc.Usage = D3D11_USAGE_DEFAULT;
		tilePoolDesc.MiscFlags = D3D11_RESOURCE_MISC_TILE_POOL;

		auto Device = d->D3D11RHI->GetDevice2();
		HRESULT hr = Device->CreateBuffer(&tilePoolDesc, nullptr, d->TiledPool.get_init_ref());
		return SUCCEEDED(hr) && d->TiledPool.is_valid();
	}

}