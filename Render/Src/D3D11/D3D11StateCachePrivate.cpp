#include <d3d11.h>
#include <dxgi1_3.h>
#include <dxgi1_4.h>
#include <dxgi1_6.h>
#include <delayimp.h>
#include "RHIPrivate/D3D11StateCachePrivate.h"

namespace RenderCore
{
	void D3D11StateCacheBase::ClearState()
	{
		if (Direct3DDeviceIMContext)
		{
			Direct3DDeviceIMContext->ClearState();
		}

#if D3D11_ALLOW_STATE_CACHE
		// Shader Resource View State Cache
		for (uint32_t ShaderFrequency = 0; ShaderFrequency < SF_NumStandardFrequencies; ShaderFrequency++)
		{
			for (uint32_t Index = 0; Index < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; Index++)
			{
				if (CurrentShaderResourceViews[ShaderFrequency][Index])
				{
					CurrentShaderResourceViews[ShaderFrequency][Index]->Release();
					CurrentShaderResourceViews[ShaderFrequency][Index] = NULL;
				}
			}
		}

		// Rasterizer State Cache
		CurrentRasterizerState = nullptr;

		// Depth Stencil State Cache
		CurrentReferenceStencil = 0;
		CurrentDepthStencilState = nullptr;
		bDepthBoundsEnabled = false;
		DepthBoundsMin = 0.0f;
		DepthBoundsMax = 1.0f;

		// Shader Cache
		CurrentVertexShader = nullptr;
		CurrentHullShader = nullptr;
		CurrentDomainShader = nullptr;
		CurrentGeometryShader = nullptr;
		CurrentPixelShader = nullptr;
		CurrentComputeShader = nullptr;

		// Blend State Cache
		CurrentBlendFactor[0] = 1.0f;
		CurrentBlendFactor[1] = 1.0f;
		CurrentBlendFactor[2] = 1.0f;
		CurrentBlendFactor[3] = 1.0f;

		ZeroMemory(&CurrentViewport[0], sizeof(D3D11_VIEWPORT) * D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
		CurrentNumberOfViewports = 0;

		CurrentBlendSampleMask = 0xffffffff;
		CurrentBlendState = nullptr;

		CurrentInputLayout = nullptr;

		ZeroMemory(CurrentVertexBuffers, sizeof(CurrentVertexBuffers));
		ZeroMemory(CurrentSamplerStates, sizeof(CurrentSamplerStates));

		CurrentIndexBuffer = nullptr;
		CurrentIndexFormat = DXGI_FORMAT_UNKNOWN;

		CurrentIndexOffset = 0;
		CurrentPrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;

		for (uint32_t Frequency = 0; Frequency < SF_NumStandardFrequencies; Frequency++)
		{
			for (uint32_t Index = 0; Index < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; Index++)
			{
				CurrentConstantBuffers[Frequency][Index].Buffer = nullptr;
				CurrentConstantBuffers[Frequency][Index].FirstConstant = 0;
				CurrentConstantBuffers[Frequency][Index].NumConstants = D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT;
			}
		}

#endif	// D3D11_ALLOW_STATE_CACHE
	}
}