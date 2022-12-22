#pragma once
#include "core/memory_manager.h"

namespace Engine
{
	class ThreadCommand
	{
	public:
		ThreadCommand(bool MustFlush = false) :_MustFlush(MustFlush) {}
		virtual ~ThreadCommand() {}
		virtual unsigned int Execute() = 0;
		virtual const wchar_t* DescribeCommand() = 0;
		inline bool GetMustFlush() const
		{
			return _MustFlush;
		}
	protected:
		bool _MustFlush;
	};

	class  CommandConstantBuffer : public win32::memory_object
	{
	public:
		enum
		{
			Constant_BUFFER_SIZE = 6 * 1024 * 1024
		};
		//VSUserConstant Type
		CommandConstantBuffer() = default;
		~CommandConstantBuffer();
		uint8_t* Assign(uint32_t uiSize);
		template<typename T>
		uint8_t* Assign( uint32_t uiRegisterNum);
		void Clear();
	
	protected:
		std::mutex _Lock;
		std::vector<uint8_t> _Buffer;
		uint32_t _uiCurBufferP = 0;
	};
}