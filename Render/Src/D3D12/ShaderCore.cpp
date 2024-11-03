#include "RHIPrivate/ShaderCore.h"

namespace RenderCore
{
	bool FShaderParameterMap::FindParameterAllocation(const std::string& ParameterName, uint16_t& OutBufferIndex, uint16_t& OutBaseIndex, uint16_t& OutSize) const
	{
		auto itFind = ParameterMap.find(ParameterName);
		itFind = ParameterMap.find(ParameterName);
		if (itFind != ParameterMap.end())
		{
			auto& Allocation = itFind->second;
			OutBufferIndex = Allocation.BufferIndex;
			OutBaseIndex = Allocation.BaseIndex;
			OutSize = Allocation.Size;

			if (Allocation.bBound)
			{
				// Can detect copy-paste errors in binding parameters.  Need to fix all the false positives before enabling.
				//UE_LOG(LogShaders, Warning, TEXT("Parameter %s was bound multiple times. Code error?"), ParameterName);
			}

			Allocation.bBound = true;
			return true;
		}
		else
		{
			return false;
		}
	}

	bool FShaderParameterMap::ContainsParameterAllocation(const std::string& ParameterName) const
	{
		return ParameterMap.count(ParameterName) > 0;
	}

	void FShaderParameterMap::AddParameterAllocation(const std::string& ParameterName, uint16_t BufferIndex, uint16_t BaseIndex, uint16_t Size, EShaderParameterType ParameterType)
	{
		assert(ParameterType < EShaderParameterType::Num);
		FParameterAllocation Allocation;
		Allocation.BufferIndex = BufferIndex;
		Allocation.BaseIndex = BaseIndex;
		Allocation.Size = Size;
		Allocation.Type = ParameterType;
		ParameterMap.insert({ ParameterName, Allocation });
	}

	void FShaderParameterMap::RemoveParameterAllocation(const std::string& ParameterName)
	{
		auto itFind = ParameterMap.find(ParameterName);
		if (itFind != ParameterMap.end())
		{
			ParameterMap.erase(itFind);
		}
	}
}