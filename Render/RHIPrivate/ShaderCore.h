#pragma once
#include "D3D12/D3D12Limits.h"
#include "RHI/RHI.h"
#include <cstring>

namespace RenderCore
{
	enum class EShaderParameterType : uint8_t
	{
		LooseData,
		UniformBuffer,
		Sampler,
		SRV,
		UAV,

		Num
	};

	struct FParameterAllocation
	{
		uint16_t BufferIndex;
		uint16_t BaseIndex;
		uint16_t Size;
		EShaderParameterType Type;
		mutable bool bBound;

		FParameterAllocation() :
			Type(EShaderParameterType::Num),
			bBound(false)
		{}
	};

	/**
 * A map of shader parameter names to registers allocated to that parameter.
 */
	class FShaderParameterMap
	{
	public:

		FShaderParameterMap()
		{}

		bool FindParameterAllocation(const std::string& ParameterName, uint16_t& OutBufferIndex, uint16_t& OutBaseIndex, uint16_t& OutSize) const;
		bool ContainsParameterAllocation(const std::string& ParameterName) const;
		void AddParameterAllocation(const std::string& ParameterName, uint16_t BufferIndex, uint16_t BaseIndex, uint16_t Size, EShaderParameterType ParameterType);
		void RemoveParameterAllocation(const std::string& ParameterName);
		/** Checks that all parameters are bound and asserts if any aren't in a debug build
		* @param InVertexFactoryType can be 0
		*/
		//void VerifyBindingsAreComplete(const TCHAR* ShaderTypeName, FShaderTarget Target, class FVertexFactoryType* InVertexFactoryType) const;

		/** Updates the hash state with the contents of this parameter map. */
		//void UpdateHash(FSHA1& HashState) const;



		//inline void GetAllParameterNames(TArray<FString>& OutNames) const
		//{
		//	ParameterMap.GenerateKeyArray(OutNames);
		//}

		inline const std::map<std::string, FParameterAllocation>& GetParameterMap() const { return ParameterMap; }

		std::map<std::string, FParameterAllocation> ParameterMap;
	};

	/** Reflection-derived bind counts and per-t# null-SRV dimensions (D3D_SRV_DIMENSION bytes) packed for runtime root layout. */
	struct FShaderCodePackedResourceCounts
	{
		static const uint8_t Key = 'p';

		bool bGlobalUniformBufferUsed{};
		uint8_t NumSamplers{};
		uint8_t NumSRVs{};
		uint8_t NumCBs{};
		uint8_t NumUAVs{};
		uint8_t SrvSlotNullViewDimension[kEngineSrvSlotNullDimensionCount]{};
	};

	using FShaderCodeResourceCounts = FShaderCodePackedResourceCounts;

	struct FShaderCompilerOutput
	{
		FShaderCompilerOutput()
		{
			// Default null SRV dimension: D3D_SRV_DIMENSION_TEXTURE2D (4) until reflection overwrites per slot.
			std::memset(ResourceCounts.SrvSlotNullViewDimension, 4, sizeof(ResourceCounts.SrvSlotNullViewDimension));
		}

		FShaderParameterMap ParameterMap;
		FShaderCodePackedResourceCounts ResourceCounts;
	};
}