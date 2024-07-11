#include "D3D11/D3D11TilePool.h"
#include "RHIPrivate/D3D11RHIPrivate.h"
#include "D3D11/D3D11RHI.h"
#include "D3D11/D3D11ReourceTraits.h"

namespace RenderCore
{
	struct D3D11MipInfo
	{
		D3D11_TILED_RESOURCE_COORDINATE startCoordinate;
		D3D11_TILE_REGION_SIZE regionSize;
	};

	struct D3D11TilePoolPrivate
	{
		D3D11DynamicRHI* D3D11RHI{ nullptr };
		win32::com_ptr<ID3D11Buffer> TiledPool;

		std::vector<D3D11_SUBRESOURCE_TILING> Tilings;
		D3D11_TILE_SHAPE TileShape;
		uint32_t NumTiles = 0;
		D3D11_PACKED_MIP_DESC PackedMipInfo;
		uint32_t SubresourceCount = 0;
		D3D11MipInfo MipInfo;
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

	bool D3D11TilePool::CreatePool(std::shared_ptr< RHITexture2D> TexRHI)
	{
		C_P(D3D11TilePool);
		D3D11Texture2D* Tex2d = RHIResourceCast(TexRHI.get());
		if (!Tex2d->GetNativeTex())
		{
			return false;
		}
		D3D11_TEXTURE2D_DESC TexDesc{};
		Tex2d->GetNativeTex()->GetDesc(&TexDesc);

		if (!(TexDesc.MiscFlags & D3D11_RESOURCE_MISC_TILED))
		{
			return false;
		}

		d->SubresourceCount = TexDesc.MipLevels;
		d->Tilings.resize(d->SubresourceCount);
		d->D3D11RHI->GetDevice2()->GetResourceTiling(Tex2d->GetNativeTex(), &d->NumTiles, &d->PackedMipInfo, &d->TileShape, &d->SubresourceCount, 0, &d->Tilings[0]);

		d->MipInfo.startCoordinate = { 0,0,0,0 };
		d->MipInfo.regionSize.Width = d->Tilings[0].WidthInTiles;
		d->MipInfo.regionSize.Height = d->Tilings[0].HeightInTiles;
		d->MipInfo.regionSize.Depth = d->Tilings[0].DepthInTiles;
		d->MipInfo.regionSize.NumTiles = d->Tilings[0].WidthInTiles * d->Tilings[0].HeightInTiles * d->Tilings[0].DepthInTiles;

		// Create the tile pool.
		D3D11_BUFFER_DESC tilePoolDesc;
		ZeroMemory(&tilePoolDesc, sizeof(tilePoolDesc));
		tilePoolDesc.ByteWidth = TileSizeInBytes * d->MipInfo.regionSize.NumTiles;
		tilePoolDesc.Usage = D3D11_USAGE_DEFAULT;
		tilePoolDesc.MiscFlags = D3D11_RESOURCE_MISC_TILE_POOL;

		auto Device = d->D3D11RHI->GetDevice2();
		HRESULT hr = Device->CreateBuffer(&tilePoolDesc, nullptr, d->TiledPool.get_init_ref());
		return SUCCEEDED(hr) && d->TiledPool.is_valid();
	}

	bool D3D11TilePool::UpdateTileMappings(std::shared_ptr< RHITexture2D> TexRHI)
	{
		C_P(D3D11TilePool);
		D3D11Texture2D* Tex2d = RHIResourceCast(TexRHI.get());
		if (!Tex2d->GetNativeTex() || !d->TiledPool.is_valid())
		{
			return false;
		}
		D3D11_TEXTURE2D_DESC TexDesc{};
		Tex2d->GetNativeTex()->GetDesc(&TexDesc);

		if (!(TexDesc.MiscFlags & D3D11_RESOURCE_MISC_TILED))
		{
			return false;
		}

		std::vector<D3D11_TILED_RESOURCE_COORDINATE> startCoordinates{ d->MipInfo.startCoordinate };
		std::vector<D3D11_TILE_REGION_SIZE> regionSizes{ d->MipInfo.regionSize };
		std::vector<uint32_t> rangeFlags{ D3D11_TILE_RANGE_NULL };
		std::vector<UINT> heapRangeStartOffsets{ 0 };
		std::vector<UINT> rangeTileCounts{ d->MipInfo.regionSize.NumTiles };

		auto Context2 = d->D3D11RHI->GetDeviceContext2();

		HRESULT hr = Context2->UpdateTileMappings(
			Tex2d->GetNativeTex(),
			startCoordinates.size(),
			&startCoordinates[0],
			&regionSizes[0],
			d->TiledPool.get(),
			rangeFlags.size(),
			&rangeFlags[0],
			&heapRangeStartOffsets[0],
			&rangeTileCounts[0],
			0
		);
		Context2->TiledResourceBarrier(nullptr, Tex2d->GetNativeTex());
		return SUCCEEDED(hr);
	}

	void D3D11TilePool::UpdateTiles(std::shared_ptr< RHITexture2D> TexRHI, const void* SourceTileData)
	{
		C_P(D3D11TilePool);
		D3D11Texture2D* Tex2d = RHIResourceCast(TexRHI.get());
		if (!Tex2d->GetNativeTex() || !d->TiledPool.is_valid())
		{
			return;
		}
		D3D11_TEXTURE2D_DESC TexDesc{};
		Tex2d->GetNativeTex()->GetDesc(&TexDesc);

		if (!(TexDesc.MiscFlags & D3D11_RESOURCE_MISC_TILED))
		{
			return;
		}

		std::vector<D3D11_TILED_RESOURCE_COORDINATE> startCoordinates{ d->MipInfo.startCoordinate };
		std::vector<D3D11_TILE_REGION_SIZE> regionSizes{ d->MipInfo.regionSize };
		auto Context2 = d->D3D11RHI->GetDeviceContext2();

		Context2->UpdateTiles(
			Tex2d->GetNativeTex(),
			startCoordinates.data(),
			regionSizes.data(),
			SourceTileData,
			0
		);
		Context2->TiledResourceBarrier(nullptr, Tex2d->GetNativeTex());
	}

}