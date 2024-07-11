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

	std::vector<UINT8> GenerateTextureData(UINT TextureWidth,UINT TextureHeight,  UINT firstMip, UINT mipCount)
	{
		const UINT TexturePixelSizeInBytes = 4;
		// Determine the size of the data required by the mips(s).
		UINT dataSize = (TextureWidth >> firstMip) * (TextureHeight >> firstMip) * TexturePixelSizeInBytes;
		if (mipCount > 1)
		{
			// If generating more than 1 mip, double the size of the texture allocation
			// (you will never need more than this).
			dataSize *= 2;
		}
		std::vector<UINT8> data(dataSize);
		UINT8* pData = &data[0];

		UINT index = 0;
		for (UINT n = 0; n < mipCount; n++)
		{
			const UINT currentMip = firstMip + n;
			const UINT width = TextureWidth >> currentMip;
			const UINT height = TextureHeight >> currentMip;
			const UINT rowPitch = width * TexturePixelSizeInBytes;
			const UINT cellPitch = std::max(rowPitch >> 3, TexturePixelSizeInBytes);    // The width of a cell in the checkboard texture.
			const UINT cellHeight = std::max(height >> 3, (UINT)1);                        // The height of a cell in the checkerboard texture.
			const UINT textureSize = rowPitch * height;

			for (UINT m = 0; m < textureSize; m += TexturePixelSizeInBytes)
			{
				UINT x = m % rowPitch;
				UINT y = m / rowPitch;
				UINT i = x / cellPitch;
				UINT j = y / cellHeight;

				if (i % 2 == j % 2)
				{
					pData[index++] = 0xff;    // R
					pData[index++] = 0x00;    // G
					pData[index++] = 0x00;    // B
					pData[index++] = 0xff;    // A
				}
				else
				{
					pData[index++] = 0xff;    // R
					pData[index++] = 0xff;    // G
					pData[index++] = 0xff;    // B
					pData[index++] = 0xff;    // A
				}
			}
		}
		return data;
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
		d->MipInfo.regionSize.bUseBox = FALSE;
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
		
		uint32_t RangeFlags = 0;
		uint32_t StartOffset = 0;

		auto Context2 = d->D3D11RHI->GetDeviceContext2();
		//UINT tileNum = d->MipInfo.regionSize.NumTiles;

		HRESULT hr = Context2->UpdateTileMappings(
			Tex2d->GetNativeTex(),
			1,
			&d->MipInfo.startCoordinate,
			&d->MipInfo.regionSize,
			d->TiledPool.get(),
			1,
			&RangeFlags,
			&StartOffset,
			nullptr,
			0);

		return SUCCEEDED(hr);
	}

	void D3D11TilePool::UpdateTiles(std::shared_ptr< RHITexture2D> TexRHI)
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

		auto Context2 = d->D3D11RHI->GetDeviceContext2();

		D3D11_TILED_RESOURCE_COORDINATE startCoordinate;
		ZeroMemory(&startCoordinate, sizeof(startCoordinate));
		D3D11_TILE_REGION_SIZE regionSize;
		ZeroMemory(&regionSize, sizeof(regionSize));
		regionSize.NumTiles = d->MipInfo.regionSize.NumTiles;
		//regionSize.NumTiles = 1;

		//std::vector<uint8_t> genData = GenerateTextureData(TexDesc.Width, TexDesc.Height, 0, 1);

		std::vector<uint8_t> testdata;
		testdata.resize(TileSizeInBytes* regionSize.NumTiles);
		memset(testdata.data(), 255, testdata.size());
		//memcpy(testdata.data(), genData.data(), genData.size());

		Context2->UpdateTiles(
			Tex2d->GetNativeTex(),
			&startCoordinate,
			&regionSize,
			testdata.data(),
			0);
	}

}