#pragma once
#include "RHI/RHICommandContext.h"
#include "D3D12/D3D12DirectCommandListManager.h"
#include "core/memory_manager.h"

namespace RenderCore
{
	class D3D12CommandContext;
	class FD3D12Device;
	// Base class used to define commands that are not device specific, or that broadcast to all devices.
	class FD3D12CommandContextBase : public RHICommandContext, public FD3D12AdapterChild
	{
	public:
		FD3D12CommandContextBase(std::weak_ptr<FD3D12Adapter> InParent, bool InIsDefaultContext, bool InIsAsyncComputeContext);

	protected:
		const bool bIsDefaultContext;
		const bool bIsAsyncComputeContext;
	};

	class D3D12CommandContext : public FD3D12CommandContextBase, public FD3D12DeviceChild
	{
	public:
		D3D12CommandContext(std::weak_ptr<FD3D12Device> InParent, bool InIsDefaultContext, bool InIsAsyncComputeContext);
		virtual ~D3D12CommandContext();
		void SetViewPort(int32_t TopLeftX, int32_t TopLeftY, int32_t SizeX, int32_t SizeY) override {};
		void SetRenderTarget(std::shared_ptr<RHITexture2D> Tex, std::shared_ptr< RHITexture2D> Depth) override {};
		virtual void SetRenderTarget(std::vector<std::shared_ptr<RHITexture2D>> Targets, std::shared_ptr< RHITexture2D> Depth) {};
		virtual void SetRenderTarget(std::shared_ptr< RHIRenderTarget> RenderTarget, int32_t IndexMip = 0) {};
		virtual void SetRenderTarget(std::shared_ptr< RHITextureCube> TextureCube, int32_t IndexView, int32_t IndexMip) {};
		virtual void Clear(std::shared_ptr< RHIRenderTarget> RenderTarget, const core::FLinearColor& Color, float Depth = 1.0f, uint8_t Stencil = 0) {};
		virtual void Clear(std::shared_ptr< RHITexture2D> RenderTarget, std::shared_ptr<RHITexture2D> DepthTarget, const core::FLinearColor& Color, float Depth = 1.0f, uint8_t Stencil = 0) {};
		virtual void Clear(std::vector<std::shared_ptr<RHITexture2D>> Targets, std::shared_ptr<RHITexture2D> DepthTarget, const core::FLinearColor& Color, float Depth = 1.0f, uint8_t Stencil = 0) {};
		virtual void Clear(std::shared_ptr< RHITextureCube> TextureCube, int32_t Face, int32_t Mip, const core::FLinearColor& Color, float Depth = 1.0f, uint8_t Stencil = 0) {};
		virtual void RHIEndDrawing() {}

		virtual void RHISetShaderSampler(EShaderFrequency ShaderType, uint32_t SamplerIndex, std::shared_ptr< RHISamplerState> NewState) {};
		virtual void RHISetRasterizerState(std::shared_ptr<RHIRasterizerState> NewStateRHI) {};
		virtual void RHISetBlendState(std::shared_ptr<RHIBlendState> NewState, const core::FLinearColor& BlendFactor) {};
		virtual void RHISetBlendFactor(const core::FLinearColor& BlendFactor) {};
		virtual void RHISetDepthStencilState(std::shared_ptr< RHIDepthStencilState> NewState, uint32_t StencilRef) {};
		virtual void RHISetStencilRef(uint32_t StencilRef) {};
		virtual void RHISetGraphicsPipelineState(const GraphicsPipelineStateInitializer& Initializer) {};
		virtual void RHIUpdateUniformBuffer(std::shared_ptr<RHIUniformBuffer> UniformBufferRHI, const void* Contents) {};
		virtual void RHISetShaderTexture(EShaderFrequency ShaderType, uint32_t TextureIndex, std::shared_ptr<RHITexture2D> Texture2DRHI) {};
		virtual void RHISetShaderTexture(EShaderFrequency ShaderType, uint32_t TextureIndex, std::shared_ptr<RHITextureCube> TextureCubeRHI) {};
		virtual void RHISetUAVParameter(uint32_t UAVIndex, std::shared_ptr<RHIUnorderedAccessView> UAV) {};
		virtual void RHISetShaderUniformBuffer(EShaderFrequency ShaderType, uint32_t BufferIndex, std::shared_ptr<RHIUniformBuffer> UniformBufferRHI) {};
		virtual void DrawPrimitive(std::shared_ptr<RHIVertexBuffer> VertexBufferRHI, std::shared_ptr<RHIIndexBuffer> IndexBufferRHI) {};
		virtual void DrawPrimitive(std::shared_ptr<RHIVertexBuffer> VertexBufferRHI) {};
		virtual void DrawPrimitive(const std::array<std::shared_ptr<RHIVertexBuffer>, VT_Max>& VertexBufferArrayRHI, std::shared_ptr<RHIIndexBuffer> IndexBufferRHI) {};
		virtual void Draw(uint32_t VertexCount, uint32_t VertexStartOffset = 0) {};
		virtual void GenerateMips(std::shared_ptr<RHITextureCube> TextureCubeRHI) {};
		virtual void RHISetComputePipelineState(const ComputePipelineStateInitializer& Initializer) {};
		virtual void RHIDispatchComputeShader(uint32_t ThreadGroupCountX, uint32_t ThreadGroupCountY, uint32_t ThreadGroupCountZ) {};
		virtual void RHICopyResource(std::shared_ptr< RHITexture2D> DstTex, std::shared_ptr< RHITexture2D> SrcTex) {};
		virtual bool UpdateTileMappings(std::shared_ptr< RHITilePool> TilePool, std::shared_ptr< RHITexture2D> TexRHI) { return false; };
		virtual void UpdateTiles(std::shared_ptr< RHITilePool> TilePool, std::shared_ptr< RHITexture2D> TexRHI, std::shared_ptr<uint8_t> Data) {};


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
		FD3D12CommandListManager& GetCommandListManager();
		void SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type, ID3D12DescriptorHeap* HeapPtr) {}

		// Cycle to a new command list, but don't execute the current one yet.
		void OpenCommandList();
		void CloseCommandList();
	private:
		// If necessary, this gets a new command allocator for this context.
		void ConditionalObtainCommandAllocator();

		// Handles to the command list and direct command allocator this context owns (granted by the command list manager/command allocator manager), and a direct pointer to the D3D command list/command allocator.
		D3D12CommandListHandle CommandListHandle;
		D3D12CommandAllocator* CommandAllocator;
		FD3D12CommandAllocatorManager CommandAllocatorManager;
	};
}