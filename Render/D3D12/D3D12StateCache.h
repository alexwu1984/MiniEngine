#pragma once
#include "RHIPrivate/D3D12RHIPrivate.h"

namespace RenderCore
{
	template<typename ResourceSlotMask>
	struct FD3D12ResourceCache
	{
		static inline void CleanSlot(ResourceSlotMask& SlotMask, uint32_t SlotIndex)
		{
			SlotMask &= ~((ResourceSlotMask)1 << SlotIndex);
		}

		static inline void DirtySlot(ResourceSlotMask& SlotMask, uint32_t SlotIndex)
		{
			SlotMask |= ((ResourceSlotMask)1 << SlotIndex);
		}

		static inline bool IsSlotDirty(const ResourceSlotMask& SlotMask, uint32_t SlotIndex)
		{
			return (SlotMask & ((ResourceSlotMask)1 << SlotIndex)) != 0;
		}

		// Mark a specific shader stage as dirty.
		inline void Dirty(EShaderFrequency ShaderFrequency, const ResourceSlotMask& SlotMask = -1)
		{
			Assert(ShaderFrequency < _ARRAYSIZE(DirtySlotMask));
			DirtySlotMask[ShaderFrequency] |= SlotMask;
		}

		// Mark specified bind slots, on all graphics stages, as dirty.
		inline void DirtyGraphics(const ResourceSlotMask& SlotMask = -1)
		{
			Dirty(SF_Vertex, SlotMask);
			Dirty(SF_Hull, SlotMask);
			Dirty(SF_Domain, SlotMask);
			Dirty(SF_Pixel, SlotMask);
			Dirty(SF_Geometry, SlotMask);
		}

		// Mark specified bind slots on compute as dirty.
		inline void DirtyCompute(const ResourceSlotMask& SlotMask = -1)
		{
			Dirty(SF_Compute, SlotMask);
		}

		// Mark specified bind slots on graphics and compute as dirty.
		inline void DirtyAll(const ResourceSlotMask& SlotMask = -1)
		{
			DirtyGraphics(SlotMask);
			DirtyCompute(SlotMask);
		}

		ResourceSlotMask DirtySlotMask[SF_NumStandardFrequencies];
	};

	struct FD3D12SamplerStateCache : public FD3D12ResourceCache<SamplerSlotMask>
	{
		FD3D12SamplerStateCache()
		{
			Clear();
		}

		inline void Clear()
		{
			DirtyAll();

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

	class FD3D12VertexShader;
	class FD3D12PixelShader;

	class FD3D12StateCache : public FD3D12DeviceChild
	{
	public:
		FD3D12StateCache(std::weak_ptr<FD3D12Device> InParent);
		~FD3D12StateCache() = default;

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
			m_PSDesc.RasterizerState = RasterizerDesc;
		}

		void SetBlendState(const D3D12_BLEND_DESC& BlendDesc)
		{
			memcpy(&m_PSDesc.BlendState, &BlendDesc, sizeof(BlendDesc));
		}

		void SetBlendFactor(const float BlendFactor[4])
		{
			if (memcmp(CurrentBlendFactor, BlendFactor, sizeof(CurrentBlendFactor)))
			{
				memcpy(&CurrentBlendFactor, BlendFactor, sizeof(CurrentBlendFactor));
				bNeedSetBlendFactor = true;
			}
		}

		void SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& DepthStencilState)
		{
			memcpy(&m_PSDesc.DepthStencilState, &DepthStencilState, sizeof(DepthStencilState));
		}

		void SetStencilRef(uint32_t StencilRef)
		{
			if (CurrentReferenceStencil != StencilRef)
			{
				CurrentReferenceStencil = StencilRef;
				bNeedSetStencilRef = true;
			}
		}

		void SetVertexShader(std::shared_ptr<FD3D12VertexShader> InVertexShader);
		void SetPixelShader(std::shared_ptr<FD3D12PixelShader> InPixelShader);

		FD3D12SamplerStateCache SamplerCache;
		D3D12_GRAPHICS_PIPELINE_STATE_DESC m_PSDesc;
		// Blend State Cache
		float CurrentBlendFactor[4]{};
		bool bNeedSetBlendFactor = false;
		uint32_t CurrentReferenceStencil = 0;
		bool bNeedSetStencilRef = false;
		std::unordered_map<uint32_t, std::shared_ptr<FD3D12VertexShader>> VertexShaders;
		std::unordered_map<uint32_t, std::shared_ptr<FD3D12PixelShader>> PixelShaders;
	};
}