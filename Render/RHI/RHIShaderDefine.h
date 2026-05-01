#pragma once
#include <cstdint>
#include <memory>
#include "RHI/DynamicRHI.h"
#include "RHI/RHICommandContext.h"
#include "RHI/RHIUniformBuffer.h"

namespace RenderCore
{
	/**
	 * CPU constant-buffer payload + RHI uniform buffer + D3D11/D3D12 `register(bN)` index.
	 * `Data` mirrors legacy BEGIN_SHADER_STRUCT layout for drop-in GET_UNIFORMDATA migration.
	 */
	template<typename TData, uint32_t kShaderRegisterB>
	class TUniformBufferBinding
	{
	public:
		using UniformDataType = TData;
		static constexpr uint32_t UniformRegisterIndex = kShaderRegisterB;

		TData Data{};

		explicit TUniformBufferBinding(DynamicRHI* rhi) : rhi_(rhi)
		{
			cb_ = rhi->RHICreateUniformBuffer(&Data, sizeof(TData));
		}

		TUniformBufferBinding(const TUniformBufferBinding&) = delete;
		TUniformBufferBinding& operator=(const TUniformBufferBinding&) = delete;
		TUniformBufferBinding(TUniformBufferBinding&&) noexcept = default;
		TUniformBufferBinding& operator=(TUniformBufferBinding&&) noexcept = default;
		~TUniformBufferBinding() = default;

		void UpdateUniformBuffer(RHICommandContext& ctx) { ctx.RHIUpdateUniformBuffer(cb_, &Data); }
		void SetShaderUniformBuffer(RHICommandContext& ctx, EShaderFrequency shaderType)
		{
			ctx.RHISetShaderUniformBuffer(shaderType, UniformRegisterIndex, cb_);
		}

		void UpdateUniformBuffer()
		{
			if (rhi_ && rhi_->GetDefaultCommandContext())
				UpdateUniformBuffer(*rhi_->GetDefaultCommandContext());
		}
		void SetShaderUniformBuffer(EShaderFrequency shaderType)
		{
			if (rhi_ && rhi_->GetDefaultCommandContext())
				SetShaderUniformBuffer(*rhi_->GetDefaultCommandContext(), shaderType);
		}

		std::shared_ptr<RHIUniformBuffer> GetRHIBuffer() const { return cb_; }

	private:
		DynamicRHI* rhi_ = nullptr;
		std::shared_ptr<RHIUniformBuffer> cb_{};
	};

	template<typename W>
	inline void RHI_UpdateAndBindUniformBufferVSPS(RHICommandContext& Ctx, W& Wrap)
	{
		Wrap.UpdateUniformBuffer(Ctx);
		Wrap.SetShaderUniformBuffer(Ctx, SF_Vertex);
		Wrap.SetShaderUniformBuffer(Ctx, SF_Pixel);
	}

	template<typename W>
	inline void RHI_UpdateAndBindUniformBuffer(RHICommandContext& Ctx, W& Wrap, EShaderFrequency Stage)
	{
		Wrap.UpdateUniformBuffer(Ctx);
		Wrap.SetShaderUniformBuffer(Ctx, Stage);
	}
}

/** Member holding one `TUniformBufferBinding`; requires `Name##Wrap` typedef in scope. */
#define DECLARE_SHADER_STRUCT_MEMBER(Name) Name##Wrap m_##Name##UniformBuffer;
#define GET_SHADER_STRUCT_MEMBER(Name) m_##Name##UniformBuffer
#define GET_UNIFORMDATA(Name) m_##Name##UniformBuffer.Data
#define GET_P_UNIFORMDATA(P, Name) (P)->GET_UNIFORMDATA(Name)
