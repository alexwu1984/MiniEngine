#pragma once
#include "RHI/RHIDefinitions.h"

namespace RenderCore
{
	class RHITilePool
	{
	public:
		RHITilePool() {}
		virtual ~RHITilePool() {}

		virtual bool CreatePool(uint32_t PoolSizeInTiles) = 0;
	};
}