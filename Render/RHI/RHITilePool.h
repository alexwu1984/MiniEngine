#pragma once
#include "RHI/RHIDefinitions.h"

namespace RenderCore
{
	class RHITexture2D;

	class RHITilePool
	{
	public:
		RHITilePool() {}
		virtual ~RHITilePool() {}

		virtual bool CreatePool(std::shared_ptr< RHITexture2D> TexRHI) = 0;
		virtual bool UpdateTileMappings(std::shared_ptr< RHITexture2D> TexRHI) = 0;
		virtual void UpdateTiles(std::shared_ptr< RHITexture2D> TexRHI, std::shared_ptr<uint8_t> Data) = 0;
	};
}