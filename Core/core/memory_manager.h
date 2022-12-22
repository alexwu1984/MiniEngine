#pragma once
#pragma warning(disable:4595)
#include "core/inc.h"
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
			RECORD_NUM = 32, //必须大于2
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
			void* pAddr[CALLSTACK_NUM];	//申请内存时候的调用堆栈信息
			unsigned int m_uiStackInfoNum;	//堆栈层数
			size_t	 m_uiSize;			//申请空间的大小
			bool m_bIsArray;				//是否是数组
			bool m_bAlignment;				//是否字节对齐
			Block* m_pPrev;				//前一个节点
			Block* m_pNext;				//后一个节点
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

		//每帧结束或者开始的时候调用
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
		/*移除这个chunk和这个chunk之前的所有chunk*/
		void FreeChunks(FTaggedMemory* NewTopChunk);

		
	};
}


inline void* operator new(size_t uiSize)
{
	return win32::memory_object::GetMemManager().Allocate(uiSize, 0, false);
}
inline void* operator new[](size_t uiSize)
{
	return win32::memory_object::GetMemManager().Allocate(uiSize, 0, true);
}

inline void operator delete (void* pvAddr)
{
	return win32::memory_object::GetMemManager().Deallocate((char*)pvAddr, 0, false);
}
inline void operator delete[](void* pvAddr)
{
	return win32::memory_object::GetMemManager().Deallocate((char*)pvAddr, 0, true);
}
