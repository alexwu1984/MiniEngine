#pragma once
#include "RHI/RHICommandContext.h"


namespace RenderCore
{
	class D3D11DynamicRHI;
	struct D3D11CommandContextP;

	class D3D11CommandContext final: public RHICommandContext
	{
	public:
		D3D11CommandContext(D3D11DynamicRHI* D3D11RHI);
		virtual ~D3D11CommandContext();

		virtual void SetViewPort(int32_t TopLeftX, int32_t TopLeftY, int32_t SizeX, int32_t SizeY) override;
		virtual void SetRenderTarget(std::shared_ptr<RHITexture2D> Tex, std::shared_ptr< RHITexture2D> Depth) override;
		virtual void SetRenderTarget(std::shared_ptr< RHIRenderTarget> RenderTarget) override;
		virtual void SetRenderTarget(std::shared_ptr< RHITextureCube> TextureCube, int32_t IndexView, int32_t IndexMip) override;
		virtual void Clear(std::shared_ptr< RHIRenderTarget> RenderTarget,const core::FLinearColor& Color, float Depth = 1.0f, uint8_t Stencil = 0) override;
		virtual void Clear(std::shared_ptr< RHITextureCube> TextureCube, const core::FLinearColor& Color, float Depth = 1.0f, uint8_t Stencil = 0) override;
		virtual void RHIEndDrawing() override;

		virtual void RHISetShaderSampler(EShaderFrequency ShaderType, uint32_t SamplerIndex, std::shared_ptr< RHISamplerState> NewState) override;
		virtual void RHISetRasterizerState(std::shared_ptr<RHIRasterizerState> NewStateRHI) override;
		virtual void RHISetBlendState(std::shared_ptr<RHIBlendState> NewState, const core::FLinearColor& BlendFactor) override;
		virtual void RHISetBlendFactor(const core::FLinearColor& BlendFactor) override;
		virtual void RHISetDepthStencilState(std::shared_ptr< RHIDepthStencilState> NewState, uint32_t StencilRef) override;
		virtual void RHISetStencilRef(uint32_t StencilRef) override;
		virtual void RHISetGraphicsPipelineState(const GraphicsPipelineStateInitializer& Initializer) override;
		virtual void RHIUpdateUniformBuffer(std::shared_ptr<RHIUniformBuffer> UniformBufferRHI, const void* Contents) override;
		virtual void RHISetShaderTexture(EShaderFrequency ShaderType, uint32_t TextureIndex, std::shared_ptr<RHITexture2D> Texture2DRHI) override;
		virtual void RHISetShaderTexture(EShaderFrequency ShaderType, uint32_t TextureIndex, std::shared_ptr<RHITextureCube> TextureCubeRHI) override;
		virtual void RHISetShaderUniformBuffer(EShaderFrequency ShaderType, uint32_t BufferIndex, std::shared_ptr<RHIUniformBuffer> UniformBufferRHI) override;
		virtual void DrawPrimitive(std::shared_ptr<RHIVertexBuffer> VertexBufferRHI, std::shared_ptr<RHIIndexBuffer> IndexBufferRHI) override;
		virtual void DrawPrimitive(std::shared_ptr<RHIVertexBuffer> VertexBufferRHI) override;
		virtual void DrawPrimitive(const std::array<std::shared_ptr<RHIVertexBuffer>, VT_Max>& VertexBufferArrayRHI, std::shared_ptr<RHIIndexBuffer> IndexBufferRHI) override;
		virtual void GenerateMips(std::shared_ptr<RHITextureCube> TextureCubeRHI);
	private:
		void ClearAllShaderResources();
		void ClearState();
	private:
		std::shared_ptr< D3D11CommandContextP> Impl;
	};
}