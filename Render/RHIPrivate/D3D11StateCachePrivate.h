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

		template <EShaderFrequency ShaderFrequency>
		void InternalSetSamplerState(uint32_t SamplerIndex, ID3D11SamplerState*& SamplerState)
		{
			switch (ShaderFrequency)
			{
			case SF_Vertex:		Direct3DDeviceIMContext->VSSetSamplers(SamplerIndex, 1, &SamplerState); break;
			case SF_Hull:		Direct3DDeviceIMContext->HSSetSamplers(SamplerIndex, 1, &SamplerState); break;
			case SF_Domain:		Direct3DDeviceIMContext->DSSetSamplers(SamplerIndex, 1, &SamplerState); break;
			case SF_Geometry:	Direct3DDeviceIMContext->GSSetSamplers(SamplerIndex, 1, &SamplerState); break;
			case SF_Pixel:		Direct3DDeviceIMContext->PSSetSamplers(SamplerIndex, 1, &SamplerState); break;
			case SF_Compute:	Direct3DDeviceIMContext->CSSetSamplers(SamplerIndex, 1, &SamplerState); break;
			}
		}

		template <EShaderFrequency ShaderFrequency>
		void SetSamplerState(ID3D11SamplerState* SamplerState, uint32_t SamplerIndex)
		{
#if D3D11_ALLOW_STATE_CACHE
			if ((CurrentSamplerStates[ShaderFrequency][SamplerIndex] != SamplerState) )
			{
				CurrentSamplerStates[ShaderFrequency][SamplerIndex] = SamplerState;
				InternalSetSamplerState<ShaderFrequency>(SamplerIndex, SamplerState);
			}
#else
			InternalSetSamplerState<ShaderFrequency>(SamplerIndex, SamplerState);
#endif
		}

		void SetRasterizerState(ID3D11RasterizerState* State)
		{
#if D3D11_ALLOW_STATE_CACHE
			if ((CurrentRasterizerState != State) )
			{
				CurrentRasterizerState = State;
				Direct3DDeviceIMContext->RSSetState(State);
			}
#else
			Direct3DDeviceIMContext->RSSetState(State);
#endif
		}

		void GetRasterizerState(ID3D11RasterizerState** RasterizerState)
		{
#if D3D11_ALLOW_STATE_CACHE
			* RasterizerState = CurrentRasterizerState;
			if (CurrentRasterizerState)
			{
				CurrentRasterizerState->AddRef();
			}
#else
			Direct3DDeviceIMContext->RSGetState(RasterizerState);
#endif
		}

		void SetBlendState(ID3D11BlendState* State, const float BlendFactor[4], uint32_t SampleMask)
		{
#if D3D11_ALLOW_STATE_CACHE
			if ((CurrentBlendState != State || CurrentBlendSampleMask != SampleMask || memcmp(CurrentBlendFactor, BlendFactor, sizeof(CurrentBlendFactor))) )
			{
				CurrentBlendState = State;
				CurrentBlendSampleMask = SampleMask;
				memcpy(CurrentBlendFactor, BlendFactor, sizeof(CurrentBlendFactor));
				Direct3DDeviceIMContext->OMSetBlendState(State, BlendFactor, SampleMask);
			}
#else
			Direct3DDeviceIMContext->OMSetBlendState(State, BlendFactor, SampleMask);
#endif
		}

		void SetBlendFactor(const float BlendFactor[4], uint32_t SampleMask)
		{
#if D3D11_ALLOW_STATE_CACHE
			if ((CurrentBlendSampleMask != SampleMask || memcmp(CurrentBlendFactor, BlendFactor, sizeof(CurrentBlendFactor))))
			{
				CurrentBlendSampleMask = SampleMask;
				memcpy(CurrentBlendFactor, BlendFactor, sizeof(CurrentBlendFactor));
				Direct3DDeviceIMContext->OMSetBlendState(CurrentBlendState, BlendFactor, SampleMask);
			}
#else
			Direct3DDeviceIMContext->OMSetBlendState(CurrentBlendState, BlendFactor, SampleMask);
#endif
		}

		void GetBlendState(ID3D11BlendState** BlendState, float BlendFactor[4], uint32_t* SampleMask)
		{
#if D3D11_ALLOW_STATE_CACHE
			* BlendState = CurrentBlendState;
			if (CurrentBlendState)
			{
				CurrentBlendState->AddRef();
			}
			*SampleMask = CurrentBlendSampleMask;
			memcpy(BlendFactor, CurrentBlendFactor, sizeof(CurrentBlendFactor));
#else
			Direct3DDeviceIMContext->OMGetBlendState(BlendState, BlendFactor, SampleMask);
#endif
		}

		void SetDepthStencilState(ID3D11DepthStencilState* State, uint32_t RefStencil)
		{
#if D3D11_ALLOW_STATE_CACHE
			if ((CurrentDepthStencilState != State || CurrentReferenceStencil != RefStencil) )
			{
				CurrentDepthStencilState = State;
				CurrentReferenceStencil = RefStencil;
				Direct3DDeviceIMContext->OMSetDepthStencilState(State, RefStencil);
			}
#else
			Direct3DDeviceIMContext->OMSetDepthStencilState(State, RefStencil);
#endif
		}

		void SetStencilRef(uint32_t RefStencil)
		{
#if D3D11_ALLOW_STATE_CACHE
			if (CurrentReferenceStencil != RefStencil )
			{
				CurrentReferenceStencil = RefStencil;
				Direct3DDeviceIMContext->OMSetDepthStencilState(CurrentDepthStencilState, RefStencil);
			}
#else
			Direct3DDeviceIMContext->OMSetDepthStencilState(CurrentDepthStencilState, RefStencil);
#endif
		}

		void GetDepthStencilState(ID3D11DepthStencilState** DepthStencilState, uint32_t* StencilRef)
		{
#if D3D11_ALLOW_STATE_CACHE
			* DepthStencilState = CurrentDepthStencilState;
			*StencilRef = CurrentReferenceStencil;
			if (CurrentDepthStencilState)
			{
				CurrentDepthStencilState->AddRef();
			}
#else
			Direct3DDeviceIMContext->OMGetDepthStencilState(DepthStencilState, StencilRef);
#endif
		}
	};
}