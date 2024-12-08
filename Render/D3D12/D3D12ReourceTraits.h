#pragma once
#include "D3D12/D3D12Texture2D.h"

namespace RenderCore
{
	template<class T>
	struct TD3D12ResourceTraits
	{
	};

	template<>
	struct TD3D12ResourceTraits<RHITexture2D>
	{
		typedef D3D12Texture2D TConcreteType;
	};

	template<typename TRHIType>
	static FORCEINLINE typename TD3D12ResourceTraits<TRHIType>::TConcreteType* RHIResourceCast(TRHIType* Resource)
	{
		return static_cast<typename TD3D12ResourceTraits<TRHIType>::TConcreteType*>(Resource);
	}
}