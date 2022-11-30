#pragma once
#include "RHI/RHIViewPort.h"

namespace RenderCore
{
	class D3D11ViewPort : public RHIViewPort
	{
	public:
		D3D11ViewPort();
		virtual ~D3D11ViewPort();
	};
}
