#include "Engine/Thread/EngineThread.h"

namespace Engine
{
	CommandConstantBuffer::~CommandConstantBuffer()
	{

	}

	uint8_t* CommandConstantBuffer::Assign(uint32_t uiSize)
	{
		std::lock_guard<std::mutex> Temp(_Lock);
		uint8_t* pTemp = NULL;
		_uiCurBufferP += uiSize;
		if (_uiCurBufferP > Constant_BUFFER_SIZE)
		{
			Assert(0);
			return nullptr;
		}

		pTemp = &_Buffer[_uiCurBufferP - uiSize];
		return pTemp;
	}

	template<typename Type>
	uint8_t* CommandConstantBuffer::Assign(uint32_t uiRegisterNum)
	{
		uint32_t uiSize = sizeof(Type) * 4 * uiRegisterNum;

		bool IsValid = false;

		if (sizeof(Type) == sizeof(float))
		{
			IsValid = true;
		}
		else if (sizeof(Type) == sizeof(int))
		{
			IsValid = true;
		}
		else if (sizeof(Type) == sizeof(bool))
		{
			IsValid = true;
		}
		Assert(IsValid);
		return Assign(uiSize);
	}

	void CommandConstantBuffer::Clear()
	{
		_uiCurBufferP = 0;
	}

}