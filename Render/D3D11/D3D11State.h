#pragma once
#include "RHI/RHIState.h"
#include "RHIPrivate/D3D11RHIDeclare.h"

namespace RenderCore
{
	class D3D11DynamicRHI;
	struct D3D11SamplerStateP;
	
	class D3D11SamplerState final : public RHISamplerState
	{
	public:
		D3D11SamplerState(D3D11DynamicRHI *D3D11RHI);
		virtual ~D3D11SamplerState();

		virtual bool CreateSamplerState(const SamplerStateInitializerRHI& Initializer) override ;
		ID3D11SamplerState* GetNativeSampleState() const;

	private:
		std::shared_ptr< D3D11SamplerStateP> Impl;
	};
}