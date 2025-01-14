#pragma once
#include "D3D12/D3D12DescriptorCache.h"

namespace RenderCore
{
	class FRootSignature;
	class D3D12CommandListHandle;
	class D3D12UniformBuffer;
	class D3D12Texture2D;

	struct FD3D12SamplerStateCache 
	{
		FD3D12SamplerStateCache()
		{
			Clear();
		}

		inline void Clear()
		{

			win32::Memzero(States);
		}

		D3D12_STATIC_SAMPLER_DESC States[SF_NumStandardFrequencies][MAX_SAMPLERS];
	};


	inline bool operator==(const D3D12_STATIC_SAMPLER_DESC& lhs, const D3D12_STATIC_SAMPLER_DESC& rhs)
	{
		return 0 == memcmp(&lhs, &rhs, sizeof(lhs));
	}

	inline bool operator != (const D3D12_STATIC_SAMPLER_DESC& lhs, const D3D12_STATIC_SAMPLER_DESC& rhs)
	{
		return !(lhs == rhs);
	}

	struct FD3D12ConstantBufferCache
	{
		FD3D12ConstantBufferCache()
		{
			Clear();
		}

		inline void Clear()
		{

			for (int32_t FrequencyIdx = 0; FrequencyIdx < SF_NumStandardFrequencies; ++FrequencyIdx)
			{
				for (int32_t SRVIdx = 0; SRVIdx < MAX_CBS; ++SRVIdx)
				{
					Buffers[FrequencyIdx][SRVIdx] = { D3D12_GPU_VIRTUAL_ADDRESS_NULL };
				}
			}
		}

		D3D12_GPU_VIRTUAL_ADDRESS Buffers[SF_NumStandardFrequencies][MAX_CBS];
	};

	struct FD3D12ShaderResourceViewCache 
	{
		FD3D12ShaderResourceViewCache()
		{
			Clear();
		}

		inline void Clear()
		{
			for (int32_t FrequencyIdx = 0; FrequencyIdx < SF_NumStandardFrequencies; ++FrequencyIdx)
			{
				for (int32_t SRVIdx = 0; SRVIdx < MAX_SRVS; ++SRVIdx)
				{
					Views[FrequencyIdx][SRVIdx] = { D3D12_GPU_VIRTUAL_ADDRESS_NULL };
				}
			}
		}

		D3D12_CPU_DESCRIPTOR_HANDLE Views[SF_NumStandardFrequencies][MAX_SRVS];
	};

	struct FD3D12UnorderedAccessViewCache 
	{
		FD3D12UnorderedAccessViewCache()
		{
			Clear();
		}

		inline void Clear()
		{
			//DirtyAll();

			//FMemory::Memzero(Views);
			//FMemory::Memzero(ResidencyHandles);

			//for (uint32& Index : StartSlot)
			//{
			//	Index = INDEX_NONE;
			//}
		}

		//FD3D12UnorderedAccessView* Views[SF_NumStandardFrequencies][MAX_UAVS];
		//FD3D12ResidencyHandle* ResidencyHandles[SF_NumStandardFrequencies][MAX_UAVS];
		//uint32 StartSlot[SF_NumStandardFrequencies];
	};

	class FD3D12VertexShader;
	class FD3D12PixelShader;
	class D3D12UniformBuffer;
	class RHITexture2D;
	class D3D12RenderTarget;
	class D3D12TextureCube;

	class FD3D12StateCache : public FD3D12DeviceChild
	{
	public:
		FD3D12StateCache(std::weak_ptr<FD3D12Device> InParent);
		~FD3D12StateCache();

		template <EShaderFrequency ShaderFrequency>
		void SetSamplerState(const D3D12_STATIC_SAMPLER_DESC& SamplerState, uint32_t SamplerIndex)
		{
			Assert(SamplerIndex < MAX_SAMPLERS);
			auto& Samplers = SamplerCache.States[ShaderFrequency];
			if (Samplers[SamplerIndex] != SamplerState)
			{
				Samplers[SamplerIndex] = SamplerState;
			}
		}

		void SetRasterizerState(const D3D12_RASTERIZER_DESC& RasterizerDesc)
		{
			PSDesc.RasterizerState = RasterizerDesc;
		}

		void SetBlendState(const D3D12_BLEND_DESC& BlendDesc)
		{
			memcpy(&PSDesc.BlendState, &BlendDesc, sizeof(BlendDesc));
		}

		void SetBlendFactor(const float BlendFactor[4])
		{
			if (memcmp(CurrentBlendFactor, BlendFactor, sizeof(CurrentBlendFactor)))
			{
				memcpy(&CurrentBlendFactor, BlendFactor, sizeof(CurrentBlendFactor));
			}
			bNeedSetBlendFactor = true;
		}

		void SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& DepthStencilState)
		{
			memcpy(&PSDesc.DepthStencilState, &DepthStencilState, sizeof(DepthStencilState));
		}

		void SetStencilRef(uint32_t StencilRef)
		{
			if (CurrentReferenceStencil != StencilRef)
			{
				CurrentReferenceStencil = StencilRef;
			}
			bNeedSetStencilRef = true;
		}

		void SetVertexShader(std::shared_ptr<FD3D12VertexShader> InVertexShader);
		void SetPixelShader(std::shared_ptr<FD3D12PixelShader> InPixelShader);
		void SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology);
		void SetDynamicConstantBuffer(EShaderFrequency ShaderType,uint32_t BufferIndex, std::shared_ptr<D3D12UniformBuffer> UniformBuffer);
		void SetShaderResourceView(EShaderFrequency ShaderType, uint32_t TextureIndex, std::shared_ptr<D3D12Texture2D> Texture2D);
		void SetShaderResourceView(EShaderFrequency ShaderType, uint32_t TextureIndex, std::shared_ptr<D3D12TextureCube> TextureCube);
		void SetDescriptorHeap(D3D12CommandListHandle& CommandList,D3D12_DESCRIPTOR_HEAP_TYPE Type, win32::com_ptr<ID3D12DescriptorHeap> HeapPtr);
		void BindDescriptorHeaps(D3D12CommandListHandle& CommandList);
		void SetRenderTargetFormats(const std::vector<std::shared_ptr<RHITexture2D>>& Targets, std::shared_ptr< RHITexture2D> Depth);
		void SetRenderTargetFormat(const D3D12RenderTarget* RenderTarget);
		void SetRenderTargetFormat(const D3D12TextureCube* RenderTarget);
		void SetIndexBuffer(D3D12CommandListHandle& CommandList,const D3D12_INDEX_BUFFER_VIEW& View);
		void SetVertexBuffer(D3D12CommandListHandle& CommandList, uint32_t Slot, const D3D12_VERTEX_BUFFER_VIEW& View);
		void SetVertexBuffers(D3D12CommandListHandle& CommandList,
							  uint32_t StartSlot, uint32_t Count, const D3D12_VERTEX_BUFFER_VIEW View[]);

		std::shared_ptr<FRootSignature> BuildRootSignature();
		bool ApplyGraphicState(D3D12CommandListHandle& CommandList);
		void ClearState();
		void ClearRenderState();
		void CleanupUsedHeaps(uint64_t FenceValue);

		FD3D12SamplerStateCache SamplerCache;
		FD3D12ConstantBufferCache ConstantBufferCache;
		FD3D12ShaderResourceViewCache ShaderResourceViewCache;
		D3D12_GRAPHICS_PIPELINE_STATE_DESC PSDesc{};
		// Blend State Cache
		float CurrentBlendFactor[4]{};
		bool bNeedSetBlendFactor = false;
		uint32_t CurrentReferenceStencil = 0;
		bool bNeedSetStencilRef = false;
		D3D12_PRIMITIVE_TOPOLOGY CurrentPrimitiveTopology{ D3D_PRIMITIVE_TOPOLOGY_UNDEFINED };
		bool bNeedSetPrimitiveTopology = false;
		std::unordered_map<uint32_t, std::shared_ptr<FD3D12VertexShader>> VertexShaders;
		std::unordered_map<uint32_t, std::shared_ptr<FD3D12PixelShader>> PixelShaders;
		std::unordered_map<uint32_t, std::shared_ptr<FRootSignature>> RootSignatures;

		uint32_t CurrentVertexHash = 0;
		uint32_t CurrentPixelHash = 0;
		uint32_t CurrentRootHash = 0;
		
		std::map<size_t, win32::com_ptr<ID3D12PipelineState>> GraphicsPSHashMap;
		std::vector<D3D12_INPUT_ELEMENT_DESC> m_InputLayouts;
		FDynamicDescriptorHeap DynamicViewDescriptorHeap;
		win32::com_ptr<ID3D12DescriptorHeap> CurrentDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	};
}