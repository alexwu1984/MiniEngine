#pragma once
#include "RHI/RHIShdader.h"

namespace RenderCore
{
	class D3D11DynamicRHI;
	struct D3D11VertexShaderP;

	class D3D11VertexShader : public RHIVertexShader
	{
	public:
		D3D11VertexShader(D3D11DynamicRHI* D3D11RHI);
		virtual ~D3D11VertexShader();

		bool CreateLayout(const std::vector< VertexElementDesc>& ElementDescs);

	private:
		std::shared_ptr<D3D11VertexShaderP> Impl;
	};
}