#include "win/win32.h"
#include "memory_manager.h"
#include "core/system.h"
#include <DbgHelp.h>

namespace win32
{
	std::recursive_mutex memory_manager::s_MemLock;



	typedef BOOL
	(WINAPI
		* tFSymInitializeW)(
			_In_ HANDLE hProcess,
			_In_opt_ PCWSTR UserSearchPath,
			_In_ BOOL fInvadeProcess
			);
	typedef BOOL
	(WINAPI
		* tFSymGetLineFromAddr64)
		(
			IN  HANDLE                  hProcess,
			IN  DWORD64                 qwAddr,
			OUT PDWORD                  pdwDisplacement,
			OUT PIMAGEHLP_LINE64        Line64
			);

	typedef DWORD
	(WINAPI
		* tFSymGetOptions)
		(
			VOID
			);

	typedef DWORD
	(WINAPI
		* tFSymSetOptions)
		(
			IN DWORD   SymOptions
			);
	static tFSymGetLineFromAddr64 fnSymGetLineFromAddr64 = NULL;
	static tFSymGetOptions fnSymGetOptions = NULL;
	static tFSymSetOptions fnSymSetOptions = NULL;
	static tFSymInitializeW fnSymInitializeW = NULL;
	static HMODULE s_DbgHelpLib = nullptr;

	inline void MbsToWcs(wchar_t* Dest, unsigned int uiSizeInWord, const char* Source, unsigned int uiSizeInByte)
	{
		mbstowcs_s(0, Dest, uiSizeInWord, Source, uiSizeInByte);
	}

	ascii_memory::ascii_memory()
	{

	}

	ascii_memory::~ascii_memory()
	{

	}

	void* win32::ascii_memory::Allocate(size_t uiSize, size_t uiAlignment, bool bIsArray)
	{
		std::lock_guard<std::recursive_mutex> Temp(s_MemLock);
		if (uiAlignment == 0)
		{
			return malloc(uiSize);

		}
		else
		{
			return _aligned_malloc(uiSize, uiAlignment);
		}
		return NULL;
	}

	void win32::ascii_memory::Deallocate(char* pcAddr, size_t uiAlignment, bool bIsArray)
	{
		std::lock_guard<std::recursive_mutex> Temp(s_MemLock);
		if (uiAlignment == 0)
		{
			free(pcAddr);
		}
		else
		{
			_aligned_free(pcAddr);
		}
	}

	memory_object::memory_object()
	{

	}

	memory_object::~memory_object()
	{

	}

	memory_manager& memory_object::GetMemManager()
	{
#ifdef _DEBUG
		static debug_memory memory;
#else
		static ascii_memory memory;
#endif
		return memory;
	}

	memory_manager& memory_object::GetAsciiManager()
	{
		static ascii_memory memory;
		return memory;
	}

	debug_memory::debug_memory()
	{
		m_uiNumNewCalls = 0;
		m_uiNumDeleteCalls = 0;

		m_uiNumBlocks = 0;
		m_uiNumBytes = 0;
		m_uiMaxNumBytes = 0;
		m_uiMaxNumBlocks = 0;

		m_pHead = 0;
		m_pTail = 0;

		for (unsigned int i = 0; i < RECORD_NUM; i++)
		{
			m_uiSizeRecord[i] = 0;
		}
	}

	debug_memory::~debug_memory()
	{
		InitDbgHelpLib();
		PrintInfo();
		FreeDbgHelpLib();
		FreeLeakMem();
	}

	void* debug_memory::Allocate(size_t uiSize, size_t uiAlignment, bool bIsArray)
	{
		std::lock_guard<std::recursive_mutex> Temp(s_MemLock);
		assert(uiSize);

		m_uiNumNewCalls++;

		uint32_t uiExtendedSize = sizeof(Block) + sizeof(unsigned int) + uiSize + sizeof(unsigned int);

		char* pcAddr = (char*)memory_object::GetAsciiManager().Allocate(uiExtendedSize, uiAlignment, bIsArray);

		assert(pcAddr);

		Block* pBlock = (Block*)pcAddr;
		pBlock->m_uiSize = uiSize;
		pBlock->m_bIsArray = bIsArray;

		bool bAlignment = (uiAlignment > 0) ? true : false;
		pBlock->m_bAlignment = bAlignment;
		pBlock->m_uiStackInfoNum = 0;
		PVOID WinBackTrace[CALLSTACK_NUM];
		short NumFrames = RtlCaptureStackBackTrace(0, CALLSTACK_NUM, WinBackTrace, NULL);

#if _WIN64
		NumFrames -= 6;
#else
		NumFrames -= 7;
#endif

		for (short i = 1; i < NumFrames; i++)
		{
			pBlock->pAddr[i - 1] = WinBackTrace[i];
			pBlock->m_uiStackInfoNum++;
		}

		InsertBlock(pBlock);

		pcAddr += sizeof(Block);

		unsigned int* pBeginMask = (unsigned int*)(pcAddr);
		*pBeginMask = BEGIN_MASK;
		pcAddr += sizeof(unsigned int);

		unsigned int* pEndMask = (unsigned int*)(pcAddr + uiSize);
		*pEndMask = END_MASK;

		m_uiNumBlocks++;
		m_uiNumBytes += uiSize;

		if (m_uiNumBytes > m_uiMaxNumBytes)
		{
			m_uiMaxNumBytes = m_uiNumBytes;
		}
		if (m_uiNumBlocks > m_uiMaxNumBlocks)
		{
			m_uiMaxNumBlocks = m_uiNumBlocks;
		}


		unsigned int uiTwoPowerI = 1;
		int i;
		for (i = 0; i <= RECORD_NUM - 2; i++, uiTwoPowerI <<= 1)
		{
			if (uiSize <= uiTwoPowerI)
			{
				m_uiSizeRecord[i]++;
				break;
			}
		}
		if (i == RECORD_NUM - 1)
		{
			m_uiSizeRecord[i]++;
		}

		return (void*)pcAddr;
	}

	void debug_memory::Deallocate(char* pcAddr, size_t uiAlignment, bool bIsArray)
	{
		std::lock_guard<std::recursive_mutex> Temp(s_MemLock);
		m_uiNumDeleteCalls++;
		assert(pcAddr);
		pcAddr -= sizeof(unsigned int);

		unsigned int* pBeginMask = (unsigned int*)(pcAddr);
		assert(*pBeginMask == BEGIN_MASK);

		pcAddr -= sizeof(Block);

		Block* pBlock = (Block*)pcAddr;
		RemoveBlock(pBlock);

		assert(pBlock->m_bIsArray == bIsArray);
		assert(m_uiNumBlocks > 0 && m_uiNumBytes >= pBlock->m_uiSize);
		bool bAlignment = (uiAlignment > 0) ? true : false;
		assert(pBlock->m_bAlignment == bAlignment);
		unsigned int* pEndMask = (unsigned int*)(pcAddr + sizeof(Block) + sizeof(unsigned int) + pBlock->m_uiSize);
		assert(*pEndMask == END_MASK);

		m_uiNumBlocks--;
		m_uiNumBytes -= pBlock->m_uiSize;

		memory_object::GetAsciiManager().Deallocate(pcAddr, uiAlignment, bIsArray);
	}

	void debug_memory::InsertBlock(Block* pBlock)
	{
		if (m_pTail)
		{
			pBlock->m_pPrev = m_pTail;
			pBlock->m_pNext = 0;
			m_pTail->m_pNext = pBlock;
			m_pTail = pBlock;
		}
		else
		{
			pBlock->m_pPrev = 0;
			pBlock->m_pNext = 0;
			m_pHead = pBlock;
			m_pTail = pBlock;
		}
	}

	void debug_memory::RemoveBlock(Block* pBlock)
	{
		if (pBlock->m_pPrev)
		{
			pBlock->m_pPrev->m_pNext = pBlock->m_pNext;
		}
		else
		{
			m_pHead = pBlock->m_pNext;
		}

		if (pBlock->m_pNext)
		{
			pBlock->m_pNext->m_pPrev = pBlock->m_pPrev;
		}
		else
		{
			m_pTail = pBlock->m_pPrev;
		}
	}

	bool debug_memory::GetFileAndLine(const void* pAddress, wchar_t szFile[260], int& line)
	{
		IMAGEHLP_LINE64 Line;
		Line.SizeOfStruct = sizeof(Line);
		memset(&Line, 0, sizeof(Line));
		DWORD Offset = 0;

		if (fnSymGetLineFromAddr64(GetCurrentProcess(), (DWORD64)pAddress, &Offset, &Line))
		{
			MbsToWcs(szFile, MAX_PATH, Line.FileName, MAX_PATH);
			line = Line.LineNumber;

			return true;
		}
		else
		{
			DWORD error = GetLastError();

			return false;
		}
	}

	bool debug_memory::InitDbgHelpLib()
	{
		//wchar_t szDbgName[MAX_PATH];
		//GetModuleFileNameW(NULL, szDbgName, MAX_PATH);
		//wchar_t* p = (wchar_t*)_tcsrchr(szDbgName, L'\\');
		//if (p)
		//	*p = 0;
		//_tcscat_s(szDbgName, MAX_PATH, L"\\dbghelp.dll");

		// 查找当前目录的DLL
		s_DbgHelpLib = LoadLibraryW(L"dbghelp.dll");
	
		fnSymGetLineFromAddr64 = (tFSymGetLineFromAddr64)GetProcAddress(s_DbgHelpLib, "SymGetLineFromAddr64");
		fnSymGetOptions = (tFSymGetOptions)GetProcAddress(s_DbgHelpLib, "SymGetOptions");
		fnSymSetOptions = (tFSymSetOptions)GetProcAddress(s_DbgHelpLib, "SymSetOptions");
		fnSymInitializeW = (tFSymInitializeW)GetProcAddress(s_DbgHelpLib, "SymInitializeW");
		DWORD SymOpts = fnSymGetOptions();

		SymOpts |= SYMOPT_LOAD_LINES;
		SymOpts |= SYMOPT_FAIL_CRITICAL_ERRORS;
		SymOpts |= SYMOPT_DEFERRED_LOADS;
		SymOpts |= SYMOPT_EXACT_SYMBOLS;

		// This option allows for undecorated names to be handled by the symbol engine.
		SymOpts |= SYMOPT_UNDNAME;

		// Disable by default as it can be very spammy/slow.  Turn it on if you are debugging symbol look-up!
		//		SymOpts |= SYMOPT_DEBUG;

		// Not sure these are important or desirable
		//		SymOpts |= SYMOPT_ALLOW_ABSOLUTE_SYMBOLS;
		//		SymOpts |= SYMOPT_CASE_INSENSITIVE;

		fnSymSetOptions(SymOpts);
		bool Temp = fnSymInitializeW(GetCurrentProcess(), NULL, true);
		return Temp;
	}

	void debug_memory::FreeLeakMem()
	{
		Block* pBlock = m_pHead;
		while (pBlock)
		{
			Block* Temp = pBlock;
			pBlock = pBlock->m_pNext;
			free((void*)Temp);
		}
	}

	void debug_memory::PrintInfo()
	{
		core::outputDebugString(L"------------------begin print leak mem------------------\n");
		core::outputDebugString(L"Max Byte Num: %d\n", m_uiMaxNumBytes);
		core::outputDebugString(L"Max Block Num: %d\n", m_uiMaxNumBlocks);
		core::outputDebugString(L"Total Size: %d\n", m_uiNumBytes);
		core::outputDebugString(L"Block Num: %d\n", m_uiNumBlocks);
		core::outputDebugString(L"New Call: %d\n", m_uiNumNewCalls);
		core::outputDebugString(L"Delete Call: %d\n", m_uiNumDeleteCalls);
		if (m_pHead)
		{
			core::outputDebugString(L"Memory Leak:\n");
		}
		else
		{
			core::outputDebugString(L"No Memory Leak\n");
		}
		Block* pBlock = m_pHead;
		static unsigned int uiLeakNum = 0;
		while (pBlock)
		{

			uiLeakNum++;
			core::outputDebugString(L"+++++++++++++++++++Leak %d+++++++++++++++++++\n", uiLeakNum);
			core::outputDebugString(L"Size: %d\n", pBlock->m_uiSize);
			core::outputDebugString(L"Is Array:%d\n", pBlock->m_bIsArray);
			wchar_t szFile[MAX_PATH];
			int	  line;
			for (unsigned int i = 0; i < pBlock->m_uiStackInfoNum; i++)
			{

				if (!GetFileAndLine(pBlock->pAddr[i], szFile, line))
				{
					break;
				}
				core::outputDebugString(L"%s(%d)\n", szFile, line);

			}
			core::outputDebugString(L"+++++++++++++++++++Leak %d+++++++++++++++++++\n", uiLeakNum);
			pBlock = pBlock->m_pNext;
		}
		core::outputDebugString(L"leak block total num : %d\n", uiLeakNum);

		core::outputDebugString(L"------------------end print leak mem------------------\n");
	}

	void debug_memory::FreeDbgHelpLib()
	{
		if (s_DbgHelpLib != NULL)
		{
			FreeLibrary(s_DbgHelpLib);
			s_DbgHelpLib = NULL;
		}


		fnSymGetLineFromAddr64 = NULL;
		fnSymGetOptions = NULL;
		fnSymSetOptions = NULL;
		fnSymInitializeW = NULL;
	}

}



