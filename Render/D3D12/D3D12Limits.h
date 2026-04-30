#pragma once
#include <cstdint>
/** D3D12 root / descriptor table limits (shared by Util and RHIPrivate). */

#define MAX_SRVS		48

/** Size of per-t# null-SRV dimension table (reflection); must be >= MAX_SRVS. */
constexpr std::uint32_t kEngineSrvSlotNullDimensionCount = 64;
static_assert(kEngineSrvSlotNullDimensionCount >= MAX_SRVS, "kEngineSrvSlotNullDimensionCount must cover MAX_SRVS");

#define MAX_SAMPLERS	16
#define MAX_UAVS		16
#define MAX_CBS			16
#define MAX_ROOT_CBVS	MAX_CBS

#define WINDOWS_DEFAULT_NUM_BACK_BUFFERS 3
