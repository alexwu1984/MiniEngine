#pragma once
/** D3D12 root / descriptor table limits (shared by Util and RHIPrivate). */

#define MAX_SRVS		48
#define MAX_SAMPLERS	16
#define MAX_UAVS		16
#define MAX_CBS			16
#define MAX_ROOT_CBVS	MAX_CBS

#define WINDOWS_DEFAULT_NUM_BACK_BUFFERS 3
