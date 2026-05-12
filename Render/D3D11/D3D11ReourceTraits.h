#pragma once
#include "D3D11/D3D11IndexBuffer.h"
#include "D3D11/D3D11VertexBuffer.h"
#include "D3D11/D3D11UniformBuffer.h"
#include "D3D11/D3D11Texture2D.h"
#include "D3D11/D3D11Texture1D.h"
#include "D3D11/D3D11RenderTarget.h"
#include "D3D11/D3D11Shader.h"
#include "D3D11/D3D11State.h"
#include "D3D11/D3D11TextureCube.h"
#include "D3D11/D3D11UnorderedAccessView.h"
#include "D3D11/D3D11StructuredBuffer.h"
#include "D3D11/D3D11TilePool.h"

namespace RenderCore
{
	template<class T>
	struct TD3D11ResourceTraits
	{
	};

	template<>
	struct TD3D11ResourceTraits<RHIVertexBuffer>
	{
		typedef D3D11VertexBuffer TConcreteType;
	};

	template<>
	struct TD3D11ResourceTraits<RHIIndexBuffer>
	{
		typedef D3D11IndexBuffer TConcreteType;
	};

	template<>
	struct TD3D11ResourceTraits<RHIUniformBuffer>
	{
		typedef D3D11UniformBuffer TConcreteType;
	};

	template<>
	struct TD3D11ResourceTraits<RHITexture2D>
	{
		typedef D3D11Texture2D TConcreteType;
	};

	template<>
	struct TD3D11ResourceTraits<RHITexture1D>
	{
		typedef D3D11Texture1D TConcreteType;
	};

	template<>
	struct TD3D11ResourceTraits<RHIRenderTarget>
	{
		typedef D3D11RenderTarget TConcreteType;
	};

	template<>
	struct TD3D11ResourceTraits<RHIVertexShader>
	{
		typedef D3D11VertexShader TConcreteType;
	};

	template<>
	struct TD3D11ResourceTraits<RHIPixelShader>
	{
		typedef D3D11PixelShader TConcreteType;
	};

	template<>
	struct TD3D11ResourceTraits<RHIComputeShader>
	{
		typedef D3D11ComputeShader TConcreteType;
	};

	template<>
	struct TD3D11ResourceTraits<RHISamplerState>
	{
		typedef D3D11SamplerState TConcreteType;
	};

	template<>
	struct TD3D11ResourceTraits<RHIRasterizerState>
	{
		typedef D3D11RasterizerState TConcreteType;
	};

	template<>
	struct TD3D11ResourceTraits<RHIBlendState>
	{
		typedef D3D11BlendState TConcreteType;
	};

	template<>
	struct TD3D11ResourceTraits<RHIDepthStencilState>
	{
		typedef D3D11DepthStencilState TConcreteType;
	};

	template<>
	struct TD3D11ResourceTraits<RHITextureCube>
	{
		typedef D3D11TextureCube TConcreteType;
	};

	template<>
	struct TD3D11ResourceTraits<RHIUnorderedAccessView>
	{
		typedef D3D11UnorderedAccessView TConcreteType;
	};

	template<>
	struct TD3D11ResourceTraits<RHIStructuredBuffer>
	{
		typedef D3D11StructuredBuffer TConcreteType;
	};

	template<>
	struct TD3D11ResourceTraits<RHITilePool>
	{
		typedef D3D11TilePool TConcreteType;
	};

	template<typename TRHIType>
	static FORCEINLINE typename TD3D11ResourceTraits<TRHIType>::TConcreteType* RHIResourceCast(TRHIType* Resource)
	{
		return static_cast<typename TD3D11ResourceTraits<TRHIType>::TConcreteType*>(Resource);
	}
}

