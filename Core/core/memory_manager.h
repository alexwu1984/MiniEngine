#pragma once
#pragma warning(disable:4595)

namespace win32
{
	template< class T > inline T Align(const T Ptr, size_t Alignment)
	{
		return (T)(((size_t)Ptr + Alignment - 1) & ~(Alignment - 1));

	}
	template< class T > inline T Align1(const T Ptr, size_t Alignment)
	{
		return (T)((size_t)Ptr + Alignment - (Ptr & (Alignment - 1)));
	}

	template< class T >
	static inline void Memzero(T& Src)
	{
		static_assert(!std::is_pointer<T>::value, "For pointers use the two parameters function");
		memset(&Src,0, sizeof(T));
	}

	class memory_manager;

	class  memory_object
	{
	public:
		memory_object();
		~memory_object();
		static memory_manager& GetMemManager();
		static memory_manager& GetAsciiManager();
	};

	class  memory_manager
	{
	public:
		memory_manager() {}
		virtual ~memory_manager() {}
		virtual void* Allocate(size_t uiSize, size_t uiAlignment, bool bIsArray) = 0;
		virtual void Deallocate(char* pcAddr, size_t uiAlignment, bool bIsArray) = 0;
	protected:
		//static VSCriticalSection ms_MemLock;
		static std::recursive_mutex s_MemLock;
	};
	class  ascii_memory : public memory_manager
	{
	public:
		ascii_memory();
		~ascii_memory();
		virtual void* Allocate(size_t uiSize, size_t uiAlignment, bool bIsArray);
		virtual void Deallocate(char* pcAddr, size_t uiAlignment, bool bIsArray);
	};

	class debug_memory : public memory_manager
	{
	public:
		debug_memory();
		~debug_memory();

		virtual void* Allocate(size_t uiSize, size_t uiAlignment, bool bIsArray);
		virtual void Deallocate(char* pcAddr, size_t uiAlignment, bool bIsArray);

	private:
		enum 
		{
			BEGIN_MASK = 0xDEADC0DE,
			END_MASK = 0xDEADC0DE,
			RECORD_NUM = 32, // must be > 2
			CALLSTACK_NUM = 32
		};


		class Block
		{
		public:
			Block()
			{
				for (unsigned int i = 0; i < CALLSTACK_NUM; i++)
				{
					pAddr[i] = nullptr;
				}
				m_pPrev = nullptr;
				m_pNext = nullptr;
				m_bAlignment = false;
				m_bIsArray = false;
				m_uiSize = 0;
				m_uiStackInfoNum = 0;
			}
			void* pAddr[CALLSTACK_NUM];	// call stack frames captured at allocation
			unsigned int m_uiStackInfoNum;	// number of valid stack frames
			size_t	 m_uiSize;			// requested allocation size
			bool m_bIsArray;				// array new[] vs single new
			bool m_bAlignment;				// aligned allocation flag
			Block* m_pPrev;				// previous block in list
			Block* m_pNext;				// next block in list
		};
		unsigned int m_uiNumNewCalls;
		unsigned int m_uiNumDeleteCalls;
		Block* m_pHead;
		Block* m_pTail;
		unsigned int m_uiNumBlocks;
		size_t m_uiNumBytes;
		size_t m_uiMaxNumBytes;
		unsigned int m_uiMaxNumBlocks;
		unsigned int m_uiSizeRecord[RECORD_NUM];
		void InsertBlock(Block* pBlock);
		void RemoveBlock(Block* pBlock);
		bool GetFileAndLine(const void* pAddress, wchar_t szFile[260], int& line);
		bool InitDbgHelpLib();
		void FreeLeakMem();
		void PrintInfo();
		void FreeDbgHelpLib();
	};


	class  stack_memory : public memory_manager
	{
	public:
		stack_memory(size_t uiDefaultChunkSize = 65536);
		~stack_memory();
		virtual void* Allocate(size_t uiSize, size_t uiAlignment, bool bIsArray);
		virtual void Deallocate(char* pcAddr, size_t uiAlignment, bool bIsArray) {}

		// call at frame begin/end
		void Clear();
		void PopMemory();

	private:

		// Types.
		struct FTaggedMemory
		{
			FTaggedMemory* Next;
			int32_t DataSize;
			uint8_t Data[1];
		};

		// Variables.
		uint8_t* Top = nullptr;				// Top of current chunk (Top<=End).
		uint8_t* End = nullptr;				// End of current chunk.
		size_t DefaultChunkSize = 0;	// Maximum chunk size to allocate.
		FTaggedMemory* TopChunk = nullptr;			// Only chunks 0..ActiveChunks-1 are valid.

		/** The memory chunks that have been allocated but are currently unused. */
		FTaggedMemory* UnusedChunks = nullptr;

		/** The number of marks on this stack. */
		int32_t NumMarks = 0;

		/**
		* Allocate a new chunk of memory of at least MinSize size,
		* and return it aligned to Align. Updates the memory stack's
		* Chunks table and ActiveChunks counter.
		*/
		uint8_t* AllocateNewChunk(int32_t MinSize);

		/** Frees the chunks above the specified chunk on the stack. */
		/* free this chunk and every chunk before it on the stack */
		void FreeChunks(FTaggedMemory* NewTopChunk);

		
	};
}

// Global allocation hooks: inline so MSVC emits one COMDAT per symbol across all TUs.
// A single non-inline definition in Core only would LNK2005 against stale Render.lib .obj
// that were built when this header still emitted the same symbols into each TU.
inline void* operator new(size_t uiSize)
{
#ifdef _DEBUG
	return win32::memory_object::GetMemManager().Allocate(uiSize, 0, false);
#else
	return malloc(uiSize);
#endif
}
inline void* operator new[](size_t uiSize)
{
#ifdef _DEBUG
	return win32::memory_object::GetMemManager().Allocate(uiSize, 0, true);
#else
	return malloc(uiSize);
#endif
}

inline void* __cdecl operator new(size_t _Size, const std::nothrow_t&) noexcept
{
#ifdef _DEBUG
	return win32::memory_object::GetMemManager().Allocate(_Size, 0, false);
#else
	return malloc(_Size);
#endif
}

inline void* __cdecl operator new[](size_t _Size, const std::nothrow_t&) noexcept
{
#ifdef _DEBUG
	return win32::memory_object::GetMemManager().Allocate(_Size, 0, true);
#else
	return malloc(_Size);
#endif
}

inline void operator delete(void* pvAddr)
{
#ifdef _DEBUG
	return win32::memory_object::GetMemManager().Deallocate((char*)pvAddr, 0, false);
#else
	free(pvAddr);
#endif
}
inline void operator delete[](void* pvAddr)
{
#ifdef _DEBUG
	return win32::memory_object::GetMemManager().Deallocate((char*)pvAddr, 0, true);
#else
	free(pvAddr);
#endif
}
