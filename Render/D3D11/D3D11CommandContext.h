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
		virtual void SetRenderTarget(const std::vector<std::shared_ptr<RHITexture2D>>& Targets, std::shared_ptr< RHITexture2D> Depth) override;
		virtual void SetRenderTarget(std::shared_ptr< RHIRenderTarget> RenderTarget,int32_t IndexMip = 0) override;
		virtual void SetRenderTarget(std::shared_ptr< RHITextureCube> TextureCube, int32_t IndexView, int32_t IndexMip) override;
		virtual void Clear(std::shared_ptr< RHIRenderTarget> RenderTarget,const core::FLinearColor& Color, float Depth = 1.0f, uint8_t Stencil = 0) override;
		virtual void Clear(std::shared_ptr< RHITexture2D> RenderTarget,std::shared_ptr<RHITexture2D> DepthTarget, const core::FLinearColor& Color, float Depth = 1.0f, uint8_t Stencil = 0) override;
		virtual void Clear(std::vector<std::shared_ptr<RHITexture2D>> Targets, std::shared_ptr<RHITexture2D> DepthTarget, const core::FLinearColor& Color, float Depth = 1.0f, uint8_t Stencil = 0) override;
		virtual void Clear(std::shared_ptr< RHITextureCube> TextureCube, int32_t Face, int32_t Mip, const core::FLinearColor& Color, float Depth = 1.0f, uint8_t Stencil = 0) override;
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
		virtual void RHISetShaderStructuredBuffer(EShaderFrequency ShaderType, uint32_t SRVIndex, std::shared_ptr<RHIStructuredBuffer> BufferRHI) override;
		virtual void RHISetShaderStructuredBufferUAV(uint32_t UAVIndex, std::shared_ptr<RHIStructuredBuffer> BufferRHI) override;
		virtual void RHISetGraphicsRoot32BitConstants(uint32_t RootParameterIndex, uint32_t Num32BitValues, const void* SrcData, uint32_t DestOffsetIn32BitValues = 0) override;
		virtual void RHISetUAVParameter(uint32_t UAVIndex, std::shared_ptr<RHIUnorderedAccessView> UAV) override;
		virtual void DrawPrimitive(std::shared_ptr<RHIVertexBuffer> VertexBufferRHI, std::shared_ptr<RHIIndexBuffer> IndexBufferRHI) override;
		virtual void DrawPrimitive(std::shared_ptr<RHIVertexBuffer> VertexBufferRHI) override;
		virtual void DrawPrimitive(std::shared_ptr<RHIVertexBuffer> VertexBufferRHI, uint32_t VertexCount, uint32_t StartVertexLocation) override;
		virtual void DrawPrimitive(const std::array<std::shared_ptr<RHIVertexBuffer>, VT_Max>& VertexBufferArrayRHI, std::shared_ptr<RHIIndexBuffer> IndexBufferRHI) override;
		virtual void DrawPrimitiveInstanced(std::shared_ptr<RHIVertexBuffer> VertexBufferRHI, std::shared_ptr<RHIIndexBuffer> IndexBufferRHI, uint32_t InstanceCount, uint32_t StartInstanceLocation = 0) override;
		virtual void DrawPrimitiveInstanced(const std::array<std::shared_ptr<RHIVertexBuffer>, VT_Max>& VertexBufferArrayRHI, std::shared_ptr<RHIIndexBuffer> IndexBufferRHI, uint32_t InstanceCount,
											uint32_t StartInstanceLocation = 0) override;
		virtual void Draw(uint32_t VertexCount, uint32_t VertexStartOffset = 0) override;
		virtual void GenerateMips(std::shared_ptr<RHITextureCube> TextureCubeRHI) override;
		virtual void RHISetComputePipelineState(const ComputePipelineStateInitializer& Initializer) override;
		virtual void RHIDispatchComputeShader(uint32_t ThreadGroupCountX, uint32_t ThreadGroupCountY, uint32_t ThreadGroupCountZ) override;
		virtual void RHICopyResource(std::shared_ptr< RHITexture2D> DstTex, std::shared_ptr< RHITexture2D> SrcTex) override;
		virtual void RHICopyResource2D(std::shared_ptr< RHITexture2D> DstTex, std::shared_ptr< RHITexture2D> SrcTex, core::vec4u rect) override;
		virtual void RDGApplyPassBeginBarriers(const FRDGTextureBarrierDesc* Items, size_t Count, ERDGPassQueue PassQueue) override;
		virtual void RHIRenderPassApplyDeclaredTextureBarriers(const FRDGTextureBarrierDesc* Items, size_t Count, ERDGPassQueue PassQueue) override;
		virtual void RHIRenderPassApplyDeclaredStructuredBufferBarriers(const FRDGStructuredBufferBarrierDesc* Items, size_t Count, ERDGPassQueue PassQueue) override;
		virtual void RDGBeginGpuPassTimingFrame() override;
		virtual void RDGWriteGpuTimestampAfterPass(const char* PassNameUtf8) override;
		virtual void RDGResolveGpuPassTimingsEndOfFrame() override;
		virtual void RDGTryConsumePreviousFrameGpuPassTimings(std::vector<std::pair<std::string, double>>& OutPassGpuMs) override;
		virtual bool UpdateTileMappings(std::shared_ptr< RHITilePool> TilePool, std::shared_ptr< RHITexture2D> TexRHI) override;
		virtual void UpdateTiles(std::shared_ptr< RHITilePool> TilePool, std::shared_ptr< RHITexture2D> TexRHI,std::shared_ptr<uint8_t> Data) override;
		virtual void RHIClearState() override;
	private:
		void ClearAllShaderResources();
		void ClearState();
		std::shared_ptr< D3D11CommandContextP> Impl;
	};
}