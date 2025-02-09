#pragma once
#include "RHI/RHIDefinitions.h"
namespace RenderCore
{
#define D3D11_ALLOW_STATE_CACHE 0
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
		ID3D11DeviceContext* Direct3DDeviceIMContext;
		ID3D11BlendState* CurrentBlendState;
		ID3D11DepthStencilState* CurrentDepthStencilState;
		uint16_t StreamStrides[MaxVertexElementCount];
		// Viewport
		uint32_t			CurrentNumberOfViewports;
		D3D11_VIEWPORT CurrentViewport[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
#if D3D11_ALLOW_STATE_CACHE
		
		// Shader Resource Views Cache
		ID3D11ShaderResourceView* CurrentShaderResourceViews[SF_NumStandardFrequencies][D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];

		// Rasterizer State Cache
		ID3D11RasterizerState* CurrentRasterizerState;

		// Depth Stencil State Cache
		uint32_t CurrentReferenceStencil;

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
#if D3D11_ALLOW_STATE_CACHE
			ZeroMemory(CurrentShaderResourceViews, sizeof(CurrentShaderResourceViews));
#endif
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

		void InternalSetStreamSource(ID3D11Buffer* VertexBuffer, uint32_t StreamIndex, uint32_t Stride, uint32_t Offset)
		{
#if D3D11_ALLOW_STATE_CACHE
			Assert(StreamIndex < ARRAYSIZE(CurrentVertexBuffers));
			FD3D11VertexBufferState& Slot = CurrentVertexBuffers[StreamIndex];
			if ((Slot.VertexBuffer != VertexBuffer || Slot.Offset != Offset || Slot.Stride != Stride))
			{
				Slot.VertexBuffer = VertexBuffer;
				Slot.Offset = Offset;
				Slot.Stride = Stride;
				Direct3DDeviceIMContext->IASetVertexBuffers(StreamIndex, 1, &VertexBuffer, &Stride, &Offset);
			}
#else
			Direct3DDeviceIMContext->IASetVertexBuffers(StreamIndex, 1, &VertexBuffer, &Stride, &Offset);
#endif
		}


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
		void InternalSetSetConstantBuffer(uint32_t SlotIndex, ID3D11Buffer*& ConstantBuffer)
		{
			switch (ShaderFrequency)
			{
			case SF_Vertex:		Direct3DDeviceIMContext->VSSetConstantBuffers(SlotIndex, 1, &ConstantBuffer); break;
			case SF_Hull:		Direct3DDeviceIMContext->HSSetConstantBuffers(SlotIndex, 1, &ConstantBuffer); break;
			case SF_Domain:		Direct3DDeviceIMContext->DSSetConstantBuffers(SlotIndex, 1, &ConstantBuffer); break;
			case SF_Geometry:	Direct3DDeviceIMContext->GSSetConstantBuffers(SlotIndex, 1, &ConstantBuffer); break;
			case SF_Pixel:		Direct3DDeviceIMContext->PSSetConstantBuffers(SlotIndex, 1, &ConstantBuffer); break;
			case SF_Compute:	Direct3DDeviceIMContext->CSSetConstantBuffers(SlotIndex, 1, &ConstantBuffer); break;
			}
		}

		template <EShaderFrequency ShaderFrequency>
		void InternalSetShaderResourceView(uint32_t ResourceIndex, ID3D11ShaderResourceView*& SRV)
		{
			// Set the SRV we have been given (or null).
			switch (ShaderFrequency)
			{
			case SF_Vertex:		Direct3DDeviceIMContext->VSSetShaderResources(ResourceIndex, 1, &SRV); break;
			case SF_Hull:		Direct3DDeviceIMContext->HSSetShaderResources(ResourceIndex, 1, &SRV); break;
			case SF_Domain:		Direct3DDeviceIMContext->DSSetShaderResources(ResourceIndex, 1, &SRV); break;
			case SF_Geometry:	Direct3DDeviceIMContext->GSSetShaderResources(ResourceIndex, 1, &SRV); break;
			case SF_Pixel:		Direct3DDeviceIMContext->PSSetShaderResources(ResourceIndex, 1, &SRV); break;
			case SF_Compute:	Direct3DDeviceIMContext->CSSetShaderResources(ResourceIndex, 1, &SRV); break;
			}
		}

		void InternalSetIndexBuffer(ID3D11Buffer* IndexBuffer, DXGI_FORMAT Format, uint32_t Offset)
		{
#if D3D11_ALLOW_STATE_CACHE

			if (bAlwaysSetIndexBuffers || (CurrentIndexBuffer != IndexBuffer || CurrentIndexFormat != Format || CurrentIndexOffset != Offset))
			{
				CurrentIndexBuffer = IndexBuffer;
				CurrentIndexFormat = Format;
				CurrentIndexOffset = Offset;
				Direct3DDeviceIMContext->IASetIndexBuffer(IndexBuffer, Format, Offset);
			}
#else
			Direct3DDeviceIMContext->IASetIndexBuffer(IndexBuffer, Format, Offset);
#endif
		}

		template <EShaderFrequency ShaderFrequency>
		void InternalSetShaderResourceView(ID3D11ShaderResourceView*& SRV, uint32_t ResourceIndex, ESRV_Type SrvType)
		{
#if D3D11_ALLOW_STATE_CACHE
			Assert(ResourceIndex < ARRAYSIZE(CurrentShaderResourceViews[ShaderFrequency]));
			if ((CurrentShaderResourceViews[ShaderFrequency][ResourceIndex] != SRV) )
			{
				if (SRV)
				{
					SRV->AddRef();
				}
				if (CurrentShaderResourceViews[ShaderFrequency][ResourceIndex])
				{
					CurrentShaderResourceViews[ShaderFrequency][ResourceIndex]->Release();
				}
				CurrentShaderResourceViews[ShaderFrequency][ResourceIndex] = SRV;
				InternalSetShaderResourceView<ShaderFrequency>(ResourceIndex, SRV);
			}
#else	// !D3D11_ALLOW_STATE_CACHE
			InternalSetShaderResourceView<ShaderFrequency>(ResourceIndex, SRV);
#endif
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

		void SetVertexShader(ID3D11VertexShader* Shader)
		{
#if D3D11_ALLOW_STATE_CACHE
			if ((CurrentVertexShader != Shader) )
			{
				CurrentVertexShader = Shader;
				Direct3DDeviceIMContext->VSSetShader(Shader, nullptr, 0);
			}
#else
			Direct3DDeviceIMContext->VSSetShader(Shader, nullptr, 0);
#endif
		}

		void GetVertexShader(ID3D11VertexShader** VertexShader)
		{
#if D3D11_ALLOW_STATE_CACHE
			* VertexShader = CurrentVertexShader;
			if (CurrentVertexShader)
			{
				CurrentVertexShader->AddRef();
			}
#else
			Direct3DDeviceIMContext->VSGetShader(VertexShader, nullptr, nullptr);
#endif
		}

		void SetHullShader(ID3D11HullShader* Shader)
		{
#if D3D11_ALLOW_STATE_CACHE
			if ((CurrentHullShader != Shader) )
			{
				CurrentHullShader = Shader;
				Direct3DDeviceIMContext->HSSetShader(Shader, nullptr, 0);
			}
#else
			Direct3DDeviceIMContext->HSSetShader(Shader, nullptr, 0);
#endif
		}

		void GetHullShader(ID3D11HullShader** HullShader)
		{
#if D3D11_ALLOW_STATE_CACHE
			* HullShader = CurrentHullShader;
			if (CurrentHullShader)
			{
				CurrentHullShader->AddRef();
			}
#else
			Direct3DDeviceIMContext->HSGetShader(HullShader, nullptr, nullptr);
#endif
		}

		void SetDomainShader(ID3D11DomainShader* Shader)
		{
#if D3D11_ALLOW_STATE_CACHE
			if ((CurrentDomainShader != Shader) )
			{
				CurrentDomainShader = Shader;
				Direct3DDeviceIMContext->DSSetShader(Shader, nullptr, 0);
			}
#else
			Direct3DDeviceIMContext->DSSetShader(Shader, nullptr, 0);
#endif
		}

		void GetDomainShader(ID3D11DomainShader** DomainShader)
		{
#if D3D11_ALLOW_STATE_CACHE
			* DomainShader = CurrentDomainShader;
			if (CurrentDomainShader)
			{
				CurrentDomainShader->AddRef();
			}
#else
			Direct3DDeviceIMContext->DSGetShader(DomainShader, nullptr, nullptr);
#endif
		}

		void SetGeometryShader(ID3D11GeometryShader* Shader)
		{
#if D3D11_ALLOW_STATE_CACHE
			if ((CurrentGeometryShader != Shader) )
			{
				CurrentGeometryShader = Shader;
				Direct3DDeviceIMContext->GSSetShader(Shader, nullptr, 0);
			}
#else
			Direct3DDeviceIMContext->GSSetShader(Shader, nullptr, 0);
#endif
		}

		void GetGeometryShader(ID3D11GeometryShader** GeometryShader)
		{
#if D3D11_ALLOW_STATE_CACHE
			* GeometryShader = CurrentGeometryShader;
			if (CurrentGeometryShader)
			{
				CurrentGeometryShader->AddRef();
			}
#else
			Direct3DDeviceIMContext->GSGetShader(GeometryShader, nullptr, nullptr);
#endif
		}

		void SetPixelShader(ID3D11PixelShader* Shader)
		{
#if D3D11_ALLOW_STATE_CACHE
			if ((CurrentPixelShader != Shader))
			{
				CurrentPixelShader = Shader;
				Direct3DDeviceIMContext->PSSetShader(Shader, nullptr, 0);
			}
#else
			Direct3DDeviceIMContext->PSSetShader(Shader, nullptr, 0);
#endif
		}

		void GetPixelShader(ID3D11PixelShader** PixelShader)
		{
#if D3D11_ALLOW_STATE_CACHE
			* PixelShader = CurrentPixelShader;
			if (CurrentPixelShader)
			{
				CurrentPixelShader->AddRef();
			}
#else
			Direct3DDeviceIMContext->PSGetShader(PixelShader, nullptr, nullptr);
#endif
		}

		void SetComputeShader(ID3D11ComputeShader* Shader)
		{
#if D3D11_ALLOW_STATE_CACHE
			if ((CurrentComputeShader != Shader))
			{
				CurrentComputeShader = Shader;
				Direct3DDeviceIMContext->CSSetShader(Shader, nullptr, 0);
			}
#else
			Direct3DDeviceIMContext->CSSetShader(Shader, nullptr, 0);
#endif
		}

		void GetComputeShader(ID3D11ComputeShader** ComputeShader)
		{
#if D3D11_ALLOW_STATE_CACHE
			* ComputeShader = CurrentComputeShader;
			if (CurrentComputeShader)
			{
				CurrentComputeShader->AddRef();
			}
#else
			Direct3DDeviceIMContext->CSGetShader(ComputeShader, nullptr, nullptr);
#endif
		}

		void SetStreamStrides(const uint16_t* InStreamStrides)
		{
#if D3D11_ALLOW_STATE_CACHE
			memcpy(StreamStrides, InStreamStrides, sizeof(StreamStrides));
#endif
		}

		void SetInputLayout(ID3D11InputLayout* InputLayout)
		{
#if D3D11_ALLOW_STATE_CACHE
			if ((CurrentInputLayout != InputLayout) )
			{
				CurrentInputLayout = InputLayout;
				Direct3DDeviceIMContext->IASetInputLayout(InputLayout);
			}
#else
			Direct3DDeviceIMContext->IASetInputLayout(InputLayout);
#endif
		}

		void GetInputLayout(ID3D11InputLayout** InputLayout)
		{
#if D3D11_ALLOW_STATE_CACHE
			* InputLayout = CurrentInputLayout;
			if (CurrentInputLayout)
			{
				CurrentInputLayout->AddRef();
			}
#else
			Direct3DDeviceIMContext->IAGetInputLayout(InputLayout);
#endif
		}

		void SetStreamSource(ID3D11Buffer* VertexBuffer, uint32_t StreamIndex, uint32_t Stride, uint32_t Offset)
		{
			StreamStrides[StreamIndex] = Stride;
			//Assert(Stride == StreamStrides[StreamIndex]);
			InternalSetStreamSource(VertexBuffer, StreamIndex, Stride, Offset);
		}

		void SetStreamSource(ID3D11Buffer* VertexBuffer, uint32_t StreamIndex, uint32_t Offset)
		{
			InternalSetStreamSource(VertexBuffer, StreamIndex, StreamStrides[StreamIndex], Offset);
		}

		void GetStreamSources(uint32_t StartStreamIndex, uint32_t NumStreams, ID3D11Buffer** VertexBuffers, uint32_t* Strides, uint32_t* Offsets)
		{
#if D3D11_ALLOW_STATE_CACHE
			Assert(StartStreamIndex + NumStreams <= D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);
			for (uint32_t StreamLoop = 0; StreamLoop < NumStreams; StreamLoop++)
			{
				FD3D11VertexBufferState& Slot = CurrentVertexBuffers[StreamLoop + StartStreamIndex];
				VertexBuffers[StreamLoop] = Slot.VertexBuffer;
				Strides[StreamLoop] = Slot.Stride;
				Offsets[StreamLoop] = Slot.Offset;
				if (Slot.VertexBuffer)
				{
					Slot.VertexBuffer->AddRef();
				}
			}
#else
			Direct3DDeviceIMContext->IAGetVertexBuffers(StartStreamIndex, NumStreams, VertexBuffers, Strides, Offsets);
#endif
		}

public:

	void SetIndexBuffer(ID3D11Buffer* IndexBuffer, DXGI_FORMAT Format, uint32_t Offset)
	{
		InternalSetIndexBuffer(IndexBuffer, Format, Offset);
	}

	void GetIndexBuffer(ID3D11Buffer** IndexBuffer, DXGI_FORMAT* Format, uint32_t* Offset)
	{
#if D3D11_ALLOW_STATE_CACHE
		* IndexBuffer = CurrentIndexBuffer;
		*Format = CurrentIndexFormat;
		*Offset = CurrentIndexOffset;
		if (CurrentIndexBuffer)
		{
			CurrentIndexBuffer->AddRef();
		}
#else
		Direct3DDeviceIMContext->IAGetIndexBuffer(IndexBuffer, Format, Offset);
#endif
	}

	template <EShaderFrequency ShaderFrequency>
	void SetConstantBuffer(ID3D11Buffer* ConstantBuffer, uint32_t SlotIndex)
	{
#if D3D11_ALLOW_STATE_CACHE
		FD3D11ConstantBufferState& Current = CurrentConstantBuffers[ShaderFrequency][SlotIndex];
		if ((Current.Buffer != ConstantBuffer || Current.FirstConstant != 0 || Current.NumConstants != D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT))
		{
			Current.Buffer = ConstantBuffer;
			Current.FirstConstant = 0;
			Current.NumConstants = D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT;
			InternalSetSetConstantBuffer<ShaderFrequency>(SlotIndex, ConstantBuffer);
		}
#else
		InternalSetSetConstantBuffer<ShaderFrequency>(SlotIndex, ConstantBuffer);
#endif
	}

	void SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY PrimitiveTopology)
	{
#if D3D11_ALLOW_STATE_CACHE
		if ((CurrentPrimitiveTopology != PrimitiveTopology) )
		{
			CurrentPrimitiveTopology = PrimitiveTopology;
			Direct3DDeviceIMContext->IASetPrimitiveTopology(PrimitiveTopology);
		}
#else
		Direct3DDeviceIMContext->IASetPrimitiveTopology(PrimitiveTopology);
#endif
	}

	void GetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY* PrimitiveTopology)
	{
#if D3D11_ALLOW_STATE_CACHE
		* PrimitiveTopology = CurrentPrimitiveTopology;
#else
		Direct3DDeviceIMContext->IAGetPrimitiveTopology(PrimitiveTopology);
#endif
	}

	template <EShaderFrequency ShaderFrequency>
	void ClearConstantBuffers()
	{
#if D3D11_ALLOW_STATE_CACHE
		win32::Memzero(CurrentConstantBuffers[ShaderFrequency]);
#endif
		ID3D11Buffer* Empty[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = { 0 };
		switch (ShaderFrequency)
		{
		case SF_Vertex:		Direct3DDeviceIMContext->VSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, Empty); break;
		case SF_Hull:		Direct3DDeviceIMContext->HSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, Empty); break;
		case SF_Domain:		Direct3DDeviceIMContext->DSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, Empty); break;
		case SF_Geometry:	Direct3DDeviceIMContext->GSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, Empty); break;
		case SF_Pixel:		Direct3DDeviceIMContext->PSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, Empty); break;
		case SF_Compute:	Direct3DDeviceIMContext->CSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, Empty); break;
		}
	}

	template <EShaderFrequency ShaderFrequency>
	 void SetShaderResourceView(ID3D11ShaderResourceView* SRV, uint32_t ResourceIndex, ESRV_Type SrvType = SRV_Unknown)
	{
		InternalSetShaderResourceView<ShaderFrequency>(SRV, ResourceIndex, SrvType);
	}

	};
}