#pragma once
#include "RHI/RHIDefinitions.h"
namespace RenderCore
{
#define D3D11_ALLOW_STATE_CACHE 1
	//-----------------------------------------------------------------------------
//	FD3D11StateCache Class Definition
//-----------------------------------------------------------------------------
	class D3D11CommandContext;
	class D3D11StateCacheBase
	{
		friend D3D11CommandContext;
	public:
		enum ESRV_Type
		{
			SRV_Unknown,
			SRV_Dynamic,
			SRV_Static,
		};

		bool bDepthBoundsEnabled = false;
		float DepthBoundsMin = 0.0f;
		float DepthBoundsMax = 1.0f;

	protected:
#if D3D11_ALLOW_STATE_CACHE
		ID3D11DeviceContext* Direct3DDeviceIMContext;

		// Shader Resource Views Cache
		ID3D11ShaderResourceView* CurrentShaderResourceViews[SF_NumStandardFrequencies][D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];

		// Rasterizer State Cache
		ID3D11RasterizerState* CurrentRasterizerState;

		// Depth Stencil State Cache
		uint32_t CurrentReferenceStencil;
		ID3D11DepthStencilState* CurrentDepthStencilState;

		// Shader Cache
		ID3D11VertexShader* CurrentVertexShader;
		ID3D11HullShader* CurrentHullShader;
		ID3D11DomainShader* CurrentDomainShader;
		ID3D11GeometryShader* CurrentGeometryShader;
		ID3D11PixelShader* CurrentPixelShader;
		ID3D11ComputeShader* CurrentComputeShader;

		// Blend State Cache
		float CurrentBlendFactor[4];
		uint32_t CurrentBlendSampleMask;
		ID3D11BlendState* CurrentBlendState;

		// Viewport
		uint32_t			CurrentNumberOfViewports;
		D3D11_VIEWPORT CurrentViewport[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];


		// Vertex Buffer State
		struct FD3D11VertexBufferState
		{
			ID3D11Buffer* VertexBuffer;
			uint32_t Stride;
			uint32_t Offset;
		} CurrentVertexBuffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];

		// Index Buffer State
		ID3D11Buffer* CurrentIndexBuffer;
		DXGI_FORMAT CurrentIndexFormat;
		uint32_t CurrentIndexOffset;

		// Primitive Topology State
		D3D11_PRIMITIVE_TOPOLOGY CurrentPrimitiveTopology;

		// Input Layout State
		ID3D11InputLayout* CurrentInputLayout;

		uint16_t StreamStrides[MaxVertexElementCount];

		// Sampler State
		ID3D11SamplerState* CurrentSamplerStates[SF_NumStandardFrequencies][D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];

		// Constant Buffer State
		struct FD3D11ConstantBufferState
		{
			ID3D11Buffer* Buffer;
			uint32_t FirstConstant;
			uint32_t NumConstants;
		} CurrentConstantBuffers[SF_NumStandardFrequencies][D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

		bool bAlwaysSetIndexBuffers;
#endif
	public:
		D3D11StateCacheBase()
			: Direct3DDeviceIMContext(nullptr)
		{
			ZeroMemory(CurrentShaderResourceViews, sizeof(CurrentShaderResourceViews));
		}

		~D3D11StateCacheBase()
		{
		}

		void Init(ID3D11DeviceContext* InDeviceContext, bool bInAlwaysSetIndexBuffers = false)
		{
			SetContext(InDeviceContext);
		}

		void SetContext(ID3D11DeviceContext* InDeviceContext)
		{
			Direct3DDeviceIMContext = InDeviceContext;
			ClearState();
			
		}

		void ClearState();
	};
}