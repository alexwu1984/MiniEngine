#pragma once
#include "RHI/RHIDefinitions.h"
#include "core/color.h"
#include "RHI/RHIPipeLineState.h"
#include "core/vec4.h"

namespace RenderCore
{
	class RHITexture2D;
	class RHIRenderTarget;
	class RHIPixelShader;
	class RHISamplerState;
	class RHIRasterizerState;
	class RHIBlendState;
	class RHIDepthStencilState;
	class RHIUniformBuffer;
	class RHIVertexBuffer;
	class RHIIndexBuffer;
	class RHITextureCube;
	class RHIComputeShader;
	class RHIViewPort;

	class RHICommandContext
	{
	public:
		RHICommandContext() = default;
		virtual ~RHICommandContext() {}

		virtual void SetViewPort(int32_t TopLeftX, int32_t TopLeftY, int32_t SizeX, int32_t SizeY) = 0;
		virtual void SetRenderTarget(std::shared_ptr<RHITexture2D> Tex, std::shared_ptr< RHITexture2D> Depth) = 0;
		virtual void SetRenderTarget(const std::vector<std::shared_ptr<RHITexture2D>>& Targets, std::shared_ptr< RHITexture2D> Depth) = 0;
		virtual void SetRenderTarget(std::shared_ptr< RHIRenderTarget> RenderTarget, int32_t IndexMip = 0) = 0;
		virtual void SetRenderTarget(std::shared_ptr< RHITextureCube> TextureCube, int32_t IndexView, int32_t IndexMip) = 0;
		virtual void Clear(std::shared_ptr< RHIRenderTarget> RenderTarget, const core::FLinearColor& Color, float Depth = 1.0f, uint8_t Stencil = 0) = 0;
		virtual void Clear(std::shared_ptr< RHITexture2D> RenderTarget, std::shared_ptr<RHITexture2D> DepthTarget, const core::FLinearColor& Color, float Depth = 1.0f, uint8_t Stencil = 0) = 0;
		virtual void Clear(std::vector<std::shared_ptr<RHITexture2D>> Targets, std::shared_ptr<RHITexture2D> DepthTarget, const core::FLinearColor& Color, float Depth = 1.0f, uint8_t Stencil = 0) = 0;
		virtual void Clear(std::shared_ptr< RHITextureCube> TextureCube, int32_t Face, int32_t Mip, const core::FLinearColor& Color, float Depth = 1.0f, uint8_t Stencil = 0) = 0;
		virtual void RHIBeing() {};
		virtual void RHIEndDrawing() = 0;

		virtual void RHISetShaderSampler(EShaderFrequency ShaderType, uint32_t SamplerIndex, std::shared_ptr< RHISamplerState> NewState) = 0;
		virtual void RHISetRasterizerState(std::shared_ptr<RHIRasterizerState> NewStateRHI) = 0;
		virtual void RHISetBlendState(std::shared_ptr<RHIBlendState> NewState, const core::FLinearColor& BlendFactor) = 0;
		virtual void RHISetBlendFactor(const core::FLinearColor& BlendFactor)  = 0;
		virtual void RHISetDepthStencilState(std::shared_ptr< RHIDepthStencilState> NewState, uint32_t StencilRef) =0;
		virtual void RHISetStencilRef(uint32_t StencilRef)  = 0;
		virtual void RHISetGraphicsPipelineState(const GraphicsPipelineStateInitializer& Initializer) = 0;
		virtual void RHIUpdateUniformBuffer(std::shared_ptr<RHIUniformBuffer> UniformBufferRHI, const void* Contents) = 0;
		virtual void RHISetShaderTexture(EShaderFrequency ShaderType, uint32_t TextureIndex, std::shared_ptr<RHITexture2D> Texture2DRHI) = 0;
		virtual void RHISetShaderTexture(EShaderFrequency ShaderType, uint32_t TextureIndex, std::shared_ptr<RHITextureCube> TextureCubeRHI) = 0;
		virtual void RHISetUAVParameter(uint32_t UAVIndex, std::shared_ptr<RHIUnorderedAccessView> UAV) = 0;
		virtual void RHISetShaderUniformBuffer(EShaderFrequency ShaderType, uint32_t BufferIndex, std::shared_ptr<RHIUniformBuffer> UniformBufferRHI) = 0;
		/** D3D12: forwards to SetGraphicsRoot32BitConstants (small per-draw constants). Other RHIs may no-op. */
		virtual void RHISetGraphicsRoot32BitConstants(uint32_t RootParameterIndex, uint32_t Num32BitValues, const void* SrcData, uint32_t DestOffsetIn32BitValues = 0) {}
		virtual void DrawPrimitive(std::shared_ptr<RHIVertexBuffer> VertexBufferRHI, std::shared_ptr<RHIIndexBuffer> IndexBufferRHI) = 0;
		virtual void DrawPrimitive(std::shared_ptr<RHIVertexBuffer> VertexBufferRHI) = 0;
		virtual void DrawPrimitive(const std::array<std::shared_ptr<RHIVertexBuffer>, VT_Max>& VertexBufferArrayRHI, std::shared_ptr<RHIIndexBuffer> IndexBufferRHI) = 0;
		virtual void DrawPrimitiveInstanced(std::shared_ptr<RHIVertexBuffer> VertexBufferRHI, std::shared_ptr<RHIIndexBuffer> IndexBufferRHI, uint32_t InstanceCount, uint32_t StartInstanceLocation = 0) = 0;
		virtual void Draw(uint32_t VertexCount, uint32_t VertexStartOffset = 0) = 0;
		virtual void GenerateMips(std::shared_ptr<RHITextureCube> TextureCubeRHI) = 0;
		virtual void RHISetComputePipelineState(const ComputePipelineStateInitializer& Initializer) = 0;
		virtual void RHIDispatchComputeShader(uint32_t ThreadGroupCountX,uint32_t ThreadGroupCountY,uint32_t ThreadGroupCountZ) = 0;
		virtual void RHICopyResource(std::shared_ptr< RHITexture2D> DstTex, std::shared_ptr< RHITexture2D> SrcTex) = 0;
		virtual void RHICopyResource2D(std::shared_ptr< RHITexture2D> DstTex, std::shared_ptr< RHITexture2D> SrcTex,core::vec4u rect) = 0;
		virtual bool UpdateTileMappings(std::shared_ptr< RHITilePool> TilePool,std::shared_ptr< RHITexture2D> TexRHI) = 0;
		virtual void UpdateTiles(std::shared_ptr< RHITilePool> TilePool, std::shared_ptr< RHITexture2D> TexRHI,std::shared_ptr<uint8_t> Data) = 0;
		virtual void FlushCommands(bool WaitForCompletion = false) {};
		virtual void RHITransitionResource(std::shared_ptr< RHITexture2D> Tex, int32_t NewState, bool Flush = false) {};
		virtual void BeginUserMark(const char* name) {};
		virtual void EndUserMark(){};
	};

	class RHICommandMark
	{
	public:
		RHICommandMark(RHICommandContext& Command,const char* Name)
			:RHICommand(Command)
		{
			RHICommand.BeginUserMark(Name);
		}

		~RHICommandMark()
		{
			RHICommand.EndUserMark();
		}

	private:
		RHICommandContext& RHICommand;
	};
}