#pragma once
#include "D3D12/D3D12Texture2D.h"
#include "D3D12/D3D12State.h"
#include "D3D12/D3D12Shaders.h"

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

	template<>
	struct TD3D12ResourceTraits<RHISamplerState>
	{
		typedef D3D12SamplerState TConcreteType;
	};

	template<>
	struct TD3D12ResourceTraits<RHIRasterizerState>
	{
		typedef D3D12RasterizerState TConcreteType;
	};

	template<>
	struct TD3D12ResourceTraits<RHIBlendState>
	{
		typedef D3D12BlendState TConcreteType;
	};

	template<>
	struct TD3D12ResourceTraits<RHIDepthStencilState>
	{
		typedef D3D12DepthStencilState TConcreteType;
	};

	template<>
	struct TD3D12ResourceTraits<RHIVertexShader>
	{
		typedef FD3D12VertexShader TConcreteType;
	};

	template<>
	struct TD3D12ResourceTraits<RHIPixelShader>
	{
		typedef FD3D12PixelShader TConcreteType;
	};

	template<typename TRHIType>
	static FORCEINLINE typename TD3D12ResourceTraits<TRHIType>::TConcreteType* RHIResourceCast(TRHIType* Resource)
	{
		return static_cast<typename TD3D12ResourceTraits<TRHIType>::TConcreteType*>(Resource);
	}
}