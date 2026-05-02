#include "win/platform_memory.h"

namespace win32
{
	const Win32MemoryConstants& Win32MemoryConstants::GetConstants()
	{
		static Win32MemoryConstants MemoryConstants;

		if (MemoryConstants.TotalPhysical == 0)
		{
			// Gather platform memory constants.
			MEMORYSTATUSEX MemoryStatusEx;
			ZeroMemory(&MemoryStatusEx, sizeof(MemoryStatusEx));
			MemoryStatusEx.dwLength = sizeof(MemoryStatusEx);
			::GlobalMemoryStatusEx(&MemoryStatusEx);

			SYSTEM_INFO SystemInfo;
			ZeroMemory(&SystemInfo, sizeof(SystemInfo));
			::GetSystemInfo(&SystemInfo);

			MemoryConstants.TotalPhysical = MemoryStatusEx.ullTotalPhys;
			MemoryConstants.TotalVirtual = MemoryStatusEx.ullTotalVirtual;
			MemoryConstants.BinnedPageSize = SystemInfo.dwAllocationGranularity;	// Use this so we get larger 64KiB pages, instead of 4KiB
			MemoryConstants.BinnedAllocationGranularity = SystemInfo.dwPageSize; // Use 4KiB pages for more efficient use of memory - 64KiB pages don't really exist on this CPU
			MemoryConstants.OsAllocationGranularity = SystemInfo.dwAllocationGranularity;	// VirtualAlloc cannot allocate memory less than that
			MemoryConstants.PageSize = SystemInfo.dwPageSize;
			MemoryConstants.AddressLimit = math::RoundUpToPowerOfTwo64(MemoryConstants.TotalPhysical);

			MemoryConstants.TotalPhysicalGB = (uint32_t)((MemoryConstants.TotalPhysical + 1024 * 1024 * 1024 - 1) / 1024 / 1024 / 1024);
		}

		return MemoryConstants;
	}
}