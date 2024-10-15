#pragma once
#include "RHI/RHICommandContext.h"
#include "D3D12/D3D12RHICommon.h"
#include "core/memory_manager.h"

namespace RenderCore
{
	class D3D12CommandContext;
	class D3D12Device;
	// Base class used to define commands that are not device specific, or that broadcast to all devices.
	class FD3D12CommandContextBase : public RHICommandContext, public D3D12AdapterChild
	{
	public:
		FD3D12CommandContextBase(std::weak_ptr<D3D12Adapter> InParent, bool InIsDefaultContext, bool InIsAsyncComputeContext);

	private:
		virtual D3D12CommandContext* GetContext(uint32_t InGPUIndex) = 0;

		// State for begin/end draw primitive UP interface.
		struct FUserPrimitiveData
		{
			FUserPrimitiveData() { win32::Memzero(*this); }
			~FUserPrimitiveData() { assert(!VertexData && !IndexData); }

			uint32_t NumPrimitives;
			uint32_t NumVertices;
			uint32_t VertexDataStride;
			void* VertexData;

			uint32_t MinVertexIndex;
			uint32_t NumIndices;
			uint32_t IndexDataStride;
			void* IndexData;

			operator bool() const { return NumVertices != 0; }
			void Reset() { win32::Memzero(*this); }
		};

		FUserPrimitiveData PendingUP;
	};

	class D3D12CommandContext : public FD3D12CommandContextBase
	{
	public:
		D3D12CommandContext(std::weak_ptr<D3D12Adapter> InParent, bool InIsDefaultContext, bool InIsAsyncComputeContext);
		void SetViewPort(int32_t TopLeftX, int32_t TopLeftY, int32_t SizeX, int32_t SizeY) override {};
		void SetRenderTarget(std::shared_ptr<RHITexture2D> Tex, std::shared_ptr< RHITexture2D> Depth) override {};
		virtual void SetRenderTarget(std::vector<std::shared_ptr<RHITexture2D>> Targets, std::shared_ptr< RHITexture2D> Depth) {};
		virtual void SetRenderTarget(std::shared_ptr< RHIRenderTarget> RenderTarget, int32_t IndexMip = 0) {};
		virtual void SetRenderTarget(std::shared_ptr< RHITextureCube> TextureCube, int32_t IndexView, int32_t IndexMip) {};
		virtual void Clear(std::shared_ptr< RHIRenderTarget> RenderTarget, const core::FLinearColor& Color, float Depth = 1.0f, uint8_t Stencil = 0) = 0;
		virtual void Clear(std::shared_ptr< RHITexture2D> RenderTarget, std::shared_ptr<RHITexture2D> DepthTarget, const core::FLinearColor& Color, float Depth = 1.0f, uint8_t Stencil = 0) = 0;
		virtual void Clear(std::vector<std::shared_ptr<RHITexture2D>> Targets, std::shared_ptr<RHITexture2D> DepthTarget, const core::FLinearColor& Color, float Depth = 1.0f, uint8_t Stencil = 0) = 0;
		virtual void Clear(std::shared_ptr< RHITextureCube> TextureCube, int32_t Face, int32_t Mip, const core::FLinearColor& Color, float Depth = 1.0f, uint8_t Stencil = 0) = 0;
		virtual void RHIEndDrawing() = 0;

		virtual void RHISetShaderSampler(EShaderFrequency ShaderType, uint32_t SamplerIndex, std::shared_ptr< RHISamplerState> NewState) = 0;
		virtual void RHISetRasterizerState(std::shared_ptr<RHIRasterizerState> NewStateRHI) = 0;
		virtual void RHISetBlendState(std::shared_ptr<RHIBlendState> NewState, const core::FLinearColor& BlendFactor) = 0;
		virtual void RHISetBlendFactor(const core::FLinearColor& BlendFactor) = 0;
		virtual void RHISetDepthStencilState(std::shared_ptr< RHIDepthStencilState> NewState, uint32_t StencilRef) = 0;
		virtual void RHISetStencilRef(uint32_t StencilRef) = 0;
		virtual void RHISetGraphicsPipelineState(const GraphicsPipelineStateInitializer& Initializer) = 0;
		virtual void RHIUpdateUniformBuffer(std::shared_ptr<RHIUniformBuffer> UniformBufferRHI, const void* Contents) = 0;
		virtual void RHISetShaderTexture(EShaderFrequency ShaderType, uint32_t TextureIndex, std::shared_ptr<RHITexture2D> Texture2DRHI) = 0;
		virtual void RHISetShaderTexture(EShaderFrequency ShaderType, uint32_t TextureIndex, std::shared_ptr<RHITextureCube> TextureCubeRHI) = 0;
		virtual void RHISetUAVParameter(uint32_t UAVIndex, std::shared_ptr<RHIUnorderedAccessView> UAV) = 0;
		virtual void RHISetShaderUniformBuffer(EShaderFrequency ShaderType, uint32_t BufferIndex, std::shared_ptr<RHIUniformBuffer> UniformBufferRHI) = 0;
		virtual void DrawPrimitive(std::shared_ptr<RHIVertexBuffer> VertexBufferRHI, std::shared_ptr<RHIIndexBuffer> IndexBufferRHI) = 0;
		virtual void DrawPrimitive(std::shared_ptr<RHIVertexBuffer> VertexBufferRHI) = 0;
		virtual void DrawPrimitive(const std::array<std::shared_ptr<RHIVertexBuffer>, VT_Max>& VertexBufferArrayRHI, std::shared_ptr<RHIIndexBuffer> IndexBufferRHI) = 0;
		virtual void Draw(uint32_t VertexCount, uint32_t VertexStartOffset = 0) = 0;
		virtual void GenerateMips(std::shared_ptr<RHITextureCube> TextureCubeRHI) = 0;
		virtual void RHISetComputePipelineState(const ComputePipelineStateInitializer& Initializer) = 0;
		virtual void RHIDispatchComputeShader(uint32_t ThreadGroupCountX, uint32_t ThreadGroupCountY, uint32_t ThreadGroupCountZ) = 0;
		virtual void RHICopyResource(std::shared_ptr< RHITexture2D> DstTex, std::shared_ptr< RHITexture2D> SrcTex) = 0;
		virtual bool UpdateTileMappings(std::shared_ptr< RHITilePool> TilePool, std::shared_ptr< RHITexture2D> TexRHI) = 0;
		virtual void UpdateTiles(std::shared_ptr< RHITilePool> TilePool, std::shared_ptr< RHITexture2D> TexRHI, std::shared_ptr<uint8_t> Data) = 0;


		uint32_t numDraws;
		uint32_t numDispatches;
		uint32_t numClears;
		uint32_t numBarriers;
		uint32_t numCopies;
		uint32_t otherWorkCounter;

		bool HasDoneWork()
		{
			return (numDraws + numDispatches + numClears + numBarriers + numCopies + otherWorkCounter) > 0;
		}
	};
}