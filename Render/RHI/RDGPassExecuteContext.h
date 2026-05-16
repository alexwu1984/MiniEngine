#pragma once

#include <cstdint>

namespace RenderCore
{
	inline thread_local uint32_t RDG_NestedPipelineBarrierDupSuppressDepth = 0;

	inline bool RDG_IsNestedPipelineBarrierDupSuppressActive()
	{
		return RDG_NestedPipelineBarrierDupSuppressDepth != 0;
	}

	struct FRDGScopedNestedPipelineBarrierDupSuppress
	{
		bool Active = false;
		explicit FRDGScopedNestedPipelineBarrierDupSuppress(bool bEnable)
			: Active(bEnable)
		{
			if (Active)
				++RDG_NestedPipelineBarrierDupSuppressDepth;
		}
		~FRDGScopedNestedPipelineBarrierDupSuppress()
		{
			if (Active)
				--RDG_NestedPipelineBarrierDupSuppressDepth;
		}
		FRDGScopedNestedPipelineBarrierDupSuppress(const FRDGScopedNestedPipelineBarrierDupSuppress&) = delete;
		FRDGScopedNestedPipelineBarrierDupSuppress& operator=(const FRDGScopedNestedPipelineBarrierDupSuppress&) = delete;
	};
}
