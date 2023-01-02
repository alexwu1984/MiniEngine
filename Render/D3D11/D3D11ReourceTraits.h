#pragma once
#include "D3D11/D3D11IndexBuffer.h"
#include "D3D11/D3D11VertexBuffer.h"

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
}

