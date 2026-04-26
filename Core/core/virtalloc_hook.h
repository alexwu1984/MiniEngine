#pragma once

namespace core
{
	// Optional diagnostic: hooks VirtualAlloc/VirtualFree in the main module (IAT patch).
	// Enabled by command line flag: wc_hook=1
	void InitVirtualAllocHookIfRequested();
}

