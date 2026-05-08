#pragma once
#include <cstdint>

namespace RenderCore
{
/**
 * Matches D3D12_DRAW_ARGUMENTS layout consumed by DrawIndexedInstancedIndirect / ExecuteIndirect (indexed draw).
 * Pack 4 so BaseVertexLocation (int32_t) aligns with API expectations on MSVC/x64.
 */
#pragma pack(push, 4)
	struct FRHIDrawIndexedIndirectArguments
	{
		uint32_t IndexCountPerInstance = 0;
		uint32_t InstanceCount = 0;
		uint32_t StartIndexLocation = 0;
		int32_t BaseVertexLocation = 0;
		uint32_t StartInstanceLocation = 0;
	};
#pragma pack(pop)
	static_assert(sizeof(FRHIDrawIndexedIndirectArguments) == 20u, "Indirect indexed args layout");

#pragma pack(push, 4)
	struct FRHIDispatchIndirectArguments
	{
		uint32_t ThreadGroupCountX = 0;
		uint32_t ThreadGroupCountY = 0;
		uint32_t ThreadGroupCountZ = 0;
	};
#pragma pack(pop)
	static_assert(sizeof(FRHIDispatchIndirectArguments) == 12u, "Indirect dispatch args layout");
}
