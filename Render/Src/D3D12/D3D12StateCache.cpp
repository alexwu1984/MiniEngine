#include "D3D12/D3D12StateCache.h"
#include "D3D12/D3D12Limits.h"
#include "RHI/RHIThreadPolicy.h"
#include <mutex>
#include "D3D12/D3D12Shaders.h"
#include "common/crc.h"
#include "D3D12/D3D12RootSignature.h"
#include "D3D12/D3D12CommandList.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12UniformBuffer.h"
#include "D3D12/D3D12ReourceTraits.h"
#include "D3D12/D3D12RenderTarget.h"
#include "D3D12/D3D12FormatUtil.h"
#include "D3D12/D3D12TextureCube.h"
#include "core/logger.h"

namespace RenderCore
{
	namespace
	{
		static void LogApplyGraphicStateFailedOnce(const char* reason)
		{
			static std::atomic<bool> s_logged{false};
			if (s_logged.exchange(true))
				return;
			core::err() << "[D3D12] ApplyGraphicState failed (" << (reason ? reason : "?")
						<< "). Opaque mesh draws are skipped; skybox/UI may still render. "
						<< "Check earlier CreateGraphicsPipelineState HRESULT, or try shaderjit=1 / rebuild ShaderLibDX/Built/*.cso.";
		}

		/** Match RHICachedStates::DepthStateDisable for PSO fields (no bound DSV / DSVFormat UNKNOWN). */
		static void ApplyDisabledDepthStencilToPSDesc(D3D12_DEPTH_STENCIL_DESC& Out)
		{
			Out.DepthEnable = FALSE;
			Out.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
			Out.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			Out.StencilEnable = FALSE;
			Out.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
			Out.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
			Out.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
			Out.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
			Out.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
			Out.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			Out.BackFace = Out.FrontFace;
		}

		static D3D12_CPU_DESCRIPTOR_HANDLE PickNullSrvForDeclaredDimension(
			uint8_t dimByte,
			D3D12_CPU_DESCRIPTOR_HANDLE NullSrv2D,
			D3D12_CPU_DESCRIPTOR_HANDLE NullSrvCube)
		{
			switch (static_cast<D3D_SRV_DIMENSION>(dimByte))
			{
			case D3D_SRV_DIMENSION_TEXTURECUBE:
			case D3D_SRV_DIMENSION_TEXTURECUBEARRAY:
				return (NullSrvCube.ptr != 0u) ? NullSrvCube : NullSrv2D;
			default:
				return NullSrv2D;
			}
		}

		// Fewer StageDescriptorHandles calls → fewer stale table regions / CopyDescriptors work (batch staging).
		// NOTE: With GPU-based validation enabled, leaving a root descriptor table uninitialized is a hard error.
		// We therefore stage the *full table* when requested, filling null entries with a valid "null SRV" descriptor.
		static void StageAllGraphicsSrvs(FDynamicDescriptorHeap& heap, int32_t rootParamIndex, uint32_t numSrvs, const D3D12_CPU_DESCRIPTOR_HANDLE* views,
			D3D12_CPU_DESCRIPTOR_HANDLE NullSrv2D, D3D12_CPU_DESCRIPTOR_HANDLE NullSrvCube, const uint8_t* slotNullDims)
		{
			if (rootParamIndex < 0)
				return;
			if (numSrvs == 0)
				return;
			std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> tmp;
			tmp.resize(numSrvs);
			for (uint32_t i = 0; i < numSrvs; ++i)
			{
				if (views[i].ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
					tmp[i] = views[i];
				else
				{
					const uint8_t dim = (slotNullDims && i < kEngineSrvSlotNullDimensionCount) ? slotNullDims[i] : (uint8_t)D3D_SRV_DIMENSION_TEXTURE2D;
					tmp[i] = PickNullSrvForDeclaredDimension(dim, NullSrv2D, NullSrvCube);
				}
			}
			heap.SetGraphicsDescriptorHandles((UINT)rootParamIndex, 0, numSrvs, tmp.data());
		}

		static void StageGraphicsSrvRange(FDynamicDescriptorHeap& heap, int32_t rootParamIndex, uint32_t dirtyMin, uint32_t dirtyMax,
			const D3D12_CPU_DESCRIPTOR_HANDLE* views, D3D12_CPU_DESCRIPTOR_HANDLE NullSrv2D, D3D12_CPU_DESCRIPTOR_HANDLE NullSrvCube, const uint8_t* slotNullDims)
		{
			if (rootParamIndex < 0 || dirtyMin > dirtyMax)
				return;
			const uint32_t count = dirtyMax - dirtyMin + 1u;
			std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> tmp;
			tmp.resize(count);
			for (uint32_t i = 0; i < count; ++i)
			{
				const uint32_t slot = dirtyMin + i;
				if (views[slot].ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
					tmp[i] = views[slot];
				else
				{
					const uint8_t dim = (slotNullDims && slot < kEngineSrvSlotNullDimensionCount) ? slotNullDims[slot] : (uint8_t)D3D_SRV_DIMENSION_TEXTURE2D;
					tmp[i] = PickNullSrvForDeclaredDimension(dim, NullSrv2D, NullSrvCube);
				}
			}
			heap.SetGraphicsDescriptorHandles((UINT)rootParamIndex, dirtyMin, count, tmp.data());
		}

		static void StageAllComputeSrvs(FDynamicDescriptorHeap& heap, int32_t rootParamIndex, uint32_t numSrvs, const D3D12_CPU_DESCRIPTOR_HANDLE* views,
			D3D12_CPU_DESCRIPTOR_HANDLE NullSrv2D, D3D12_CPU_DESCRIPTOR_HANDLE NullSrvCube, const uint8_t* slotNullDims)
		{
			if (rootParamIndex < 0)
				return;
			if (numSrvs == 0)
				return;
			std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> tmp;
			tmp.resize(numSrvs);
			for (uint32_t i = 0; i < numSrvs; ++i)
			{
				if (views[i].ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
					tmp[i] = views[i];
				else
				{
					const uint8_t dim = (slotNullDims && i < kEngineSrvSlotNullDimensionCount) ? slotNullDims[i] : (uint8_t)D3D_SRV_DIMENSION_TEXTURE2D;
					tmp[i] = PickNullSrvForDeclaredDimension(dim, NullSrv2D, NullSrvCube);
				}
			}
			heap.SetComputeDescriptorHandles((UINT)rootParamIndex, 0, numSrvs, tmp.data());
		}

		static void StageAllComputeUavs(FDynamicDescriptorHeap& heap, int32_t rootParamIndex, uint32_t numUavs, const D3D12_CPU_DESCRIPTOR_HANDLE* views, D3D12_CPU_DESCRIPTOR_HANDLE NullUav)
		{
			if (rootParamIndex < 0)
				return;
			if (numUavs == 0)
				return;
			std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> tmp;
			tmp.resize(numUavs);
			for (uint32_t i = 0; i < numUavs; ++i)
				tmp[i] = (views[i].ptr == D3D12_GPU_VIRTUAL_ADDRESS_NULL) ? NullUav : views[i];
			heap.SetComputeDescriptorHandles((UINT)rootParamIndex, 0, numUavs, tmp.data());
		}

		static size_t HashBytesStable(const void* Data, size_t NumBytes, size_t Seed)
		{
			return (size_t)core::Crc::MemCrc32(Data, (int32_t)NumBytes, (uint32_t)Seed);
		}

		static size_t HashVertexLayoutStable(const std::vector<VertexElementDesc>& ElementDescs, size_t Seed)
		{
			for (const VertexElementDesc& E : ElementDescs)
			{
				Seed = HashBytesStable(E.SemanticName, sizeof(E.SemanticName), Seed);
				Seed = HashBytesStable(&E.SemanticIndex, sizeof(E.SemanticIndex), Seed);
				Seed = HashBytesStable(&E.Format, sizeof(E.Format), Seed);
				Seed = HashBytesStable(&E.InputSlot, sizeof(E.InputSlot), Seed);
				Seed = HashBytesStable(&E.AlignedByteOffset, sizeof(E.AlignedByteOffset), Seed);
				Seed = HashBytesStable(&E.InputSlotClass, sizeof(E.InputSlotClass), Seed);
				Seed = HashBytesStable(&E.InstanceDataStepRate, sizeof(E.InstanceDataStepRate), Seed);
			}
			return Seed;
		}

		static size_t HashGraphicsPSOStable(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& Desc,
										   uint32_t VSHash, uint32_t PSHash,
										   const std::vector<VertexElementDesc>& ElementDescs,
										   const std::string& RootSigLayoutKey)
		{
			size_t H = 0;
			H = HashBytesStable(&VSHash, sizeof(VSHash), H);
			H = HashBytesStable(&PSHash, sizeof(PSHash), H);

			// Copy stable POD fields; exclude pointer fields (pRootSignature, bytecode pointers, input layout pointer).
			H = HashBytesStable(&Desc.BlendState, sizeof(Desc.BlendState), H);
			H = HashBytesStable(&Desc.SampleMask, sizeof(Desc.SampleMask), H);
			H = HashBytesStable(&Desc.RasterizerState, sizeof(Desc.RasterizerState), H);
			H = HashBytesStable(&Desc.DepthStencilState, sizeof(Desc.DepthStencilState), H);
			H = HashBytesStable(&Desc.PrimitiveTopologyType, sizeof(Desc.PrimitiveTopologyType), H);
			H = HashBytesStable(&Desc.NumRenderTargets, sizeof(Desc.NumRenderTargets), H);
			H = HashBytesStable(&Desc.RTVFormats, sizeof(Desc.RTVFormats), H);
			H = HashBytesStable(&Desc.DSVFormat, sizeof(Desc.DSVFormat), H);
			H = HashBytesStable(&Desc.SampleDesc, sizeof(Desc.SampleDesc), H);
			H = HashBytesStable(&Desc.Flags, sizeof(Desc.Flags), H);

			H = HashVertexLayoutStable(ElementDescs, H);
			if (!RootSigLayoutKey.empty())
				H = HashBytesStable(RootSigLayoutKey.data(), RootSigLayoutKey.size(), H);
			return H;
		}

		static size_t HashComputePSOStable(const D3D12_COMPUTE_PIPELINE_STATE_DESC& Desc, uint32_t CSHash,
										   const std::string& RootSigLayoutKey)
		{
			size_t H = 0;
			H = HashBytesStable(&CSHash, sizeof(CSHash), H);
			// Exclude pointer fields: pRootSignature and CS bytecode pointers.
			H = HashBytesStable(&Desc.NodeMask, sizeof(Desc.NodeMask), H);
			H = HashBytesStable(&Desc.Flags, sizeof(Desc.Flags), H);
			if (!RootSigLayoutKey.empty())
				H = HashBytesStable(RootSigLayoutKey.data(), RootSigLayoutKey.size(), H);
			return H;
		}

		static void AppendStaticSamplerDigestToRootCacheKey(std::string& KeyName,
			uint32_t VsNumSamplers,
			uint32_t PsNumSamplers,
			uint32_t CsNumSamplers,
			const FD3D12SamplerStateCache& SamplerCache)
		{
			uint32_t h = 2166136261u;
			auto digestRange = [&](EShaderFrequency F, uint32_t n)
			{
				for (uint32_t i = 0; i < n; ++i)
					h = core::Crc::MemCrc32(&SamplerCache.States[F][i], sizeof(D3D12_STATIC_SAMPLER_DESC), (int32_t)h);
			};
			digestRange(SF_Vertex, VsNumSamplers);
			digestRange(SF_Pixel, PsNumSamplers);
			digestRange(SF_Compute, CsNumSamplers);
			KeyName.push_back('_');
			KeyName += std::to_string(static_cast<unsigned long long>(h));
		}
	}

	FD3D12StateCache::FD3D12StateCache(std::weak_ptr<FD3D12Device> InParent, std::weak_ptr<D3D12CommandContext> CommandContext)
		:FD3D12DeviceChild(InParent)
		,DynamicViewDescriptorHeap(InParent, CommandContext,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
	{
		ResetSrvDirtyTracking();
	}

	void FD3D12StateCache::ResetSrvDirtyTracking()
	{
		for (int32_t f = 0; f < SF_NumStandardFrequencies; ++f)
		{
			m_SrvDirtyMin[f] = MAX_SRVS;
			m_SrvDirtyMax[f] = 0;
			m_SrvDirtyFull[f] = false;
		}
	}

	void FD3D12StateCache::MarkSrvSlotDirty(EShaderFrequency ShaderType, uint32_t Slot)
	{
		if (ShaderType >= SF_NumStandardFrequencies)
			return;
		m_GraphicsBindDirtyMask |= kGraphicsDirtySRV;
		if (m_SrvDirtyFull[ShaderType])
			return;
		if (m_SrvDirtyMin[ShaderType] > m_SrvDirtyMax[ShaderType])
		{
			m_SrvDirtyMin[ShaderType] = Slot;
			m_SrvDirtyMax[ShaderType] = Slot;
		}
		else
		{
			m_SrvDirtyMin[ShaderType] = (std::min)(m_SrvDirtyMin[ShaderType], Slot);
			m_SrvDirtyMax[ShaderType] = (std::max)(m_SrvDirtyMax[ShaderType], Slot);
		}
	}

	void FD3D12StateCache::MarkSrvFrequencyFullyDirty(EShaderFrequency ShaderType)
	{
		if (ShaderType >= SF_NumStandardFrequencies)
			return;
		m_SrvDirtyFull[ShaderType] = true;
		m_SrvDirtyMin[ShaderType] = 0;
		m_SrvDirtyMax[ShaderType] = MAX_SRVS > 0 ? MAX_SRVS - 1u : 0u;
		m_GraphicsBindDirtyMask |= kGraphicsDirtySRV;
	}

	FD3D12StateCache::~FD3D12StateCache()
	{
		VertexShaders.clear();
		PixelShaders.clear();
		RootSignatures.clear();
		GraphicsPSHashMap.clear();
		for (int32_t index = 0; index < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++index)
			CurrentDescriptorHeaps[index] = {};
	}

	void FD3D12StateCache::SetVertexShader(std::shared_ptr<FD3D12VertexShader> InVertexShader)
	{
		const uint32_t NewHash = InVertexShader ? InVertexShader->Hash : 0u;
		if (NewHash == CurrentVertexHash)
			return;

		ShaderResourceViewCache.ClearFrequency(SF_Vertex);
		MarkSrvFrequencyFullyDirty(SF_Vertex);

		if (InVertexShader)
		{
			if (VertexShaders.count(InVertexShader->Hash)== 0)
				VertexShaders.insert({ InVertexShader->Hash, InVertexShader });
			CurrentVertexHash = InVertexShader->Hash;
			PSDesc.VS = CD3DX12_SHADER_BYTECODE(InVertexShader->Code.get());
		}
		else
		{
			CurrentVertexHash = 0;
			PSDesc.VS = CD3DX12_SHADER_BYTECODE();
		}
		MarkGraphicsLayoutDirty();
	}

	void FD3D12StateCache::SetPixelShader(std::shared_ptr<FD3D12PixelShader> InPixelShader)
	{
		const uint32_t NewHash = InPixelShader ? InPixelShader->Hash : 0u;
		if (NewHash == CurrentPixelHash)
			return;

		ShaderResourceViewCache.ClearFrequency(SF_Pixel);
		MarkSrvFrequencyFullyDirty(SF_Pixel);

		if (InPixelShader)
		{
			if(PixelShaders.count(InPixelShader->Hash) == 0)
				PixelShaders.insert({ InPixelShader->Hash, InPixelShader });
			CurrentPixelHash = InPixelShader->Hash;
			PSDesc.PS = CD3DX12_SHADER_BYTECODE(InPixelShader->Code.get());
		}
		else
		{
			CurrentPixelHash = 0;
			PSDesc.PS = CD3DX12_SHADER_BYTECODE();
		}
		MarkGraphicsLayoutDirty();
	}

	void FD3D12StateCache::SetComputeShader(std::shared_ptr<FD3D12ComputeShader> InComputeShader)
	{
		const uint32_t NewHash = InComputeShader ? InComputeShader->Hash : 0u;
		if (NewHash == CurrentComputeHash)
			return;

		ShaderResourceViewCache.ClearFrequency(SF_Compute);
		UAVCache.Clear();
		m_GraphicsBindDirtyMask |= kGraphicsDirtySRV;

		if (InComputeShader)
		{
			if (ComputeShaders.count(InComputeShader->Hash) == 0)
				ComputeShaders.insert({ InComputeShader->Hash, InComputeShader });
			CurrentComputeHash = InComputeShader->Hash;
			CSDesc.CS = CD3DX12_SHADER_BYTECODE(InComputeShader->Code.get());
		}
		else
		{
			CurrentComputeHash = 0;
			CSDesc.CS = CD3DX12_SHADER_BYTECODE();
		}
	}

	void FD3D12StateCache::SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology)
	{
		if (CurrentPrimitiveTopology != PrimitiveTopology)
		{
			CurrentPrimitiveTopology = PrimitiveTopology;
			PSDesc.PrimitiveTopologyType = D3D12PrimitiveTypeToTopologyType(PrimitiveTopology);
			MarkGraphicsLayoutDirty();
		}
		bNeedSetPrimitiveTopology = true;
	}

	void FD3D12StateCache::SetDynamicConstantBuffer(EShaderFrequency ShaderType, uint32_t BufferIndex, std::shared_ptr<D3D12UniformBuffer> UniformBuffer)
	{
		Assert(BufferIndex < MAX_CBS);
		if (!UniformBuffer)
		{
			if (ConstantBufferCache.Buffers[ShaderType][BufferIndex] != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
			{
				ConstantBufferCache.Buffers[ShaderType][BufferIndex] = D3D12_GPU_VIRTUAL_ADDRESS_NULL;
				ConstantBufferObjects[ShaderType][BufferIndex].reset();
				m_GraphicsBindDirtyMask |= kGraphicsDirtyCBV;
			}
			if (BufferIndex == 0 && ShaderType < SF_NumStandardFrequencies)
				m_RootConstantUniformBuffer[ShaderType].reset();
			return;
		}
		const D3D12_GPU_VIRTUAL_ADDRESS va = UniformBuffer->GetGPUVirtualAddress();
		if (ConstantBufferCache.Buffers[ShaderType][BufferIndex] != va)
		{
			ConstantBufferCache.Buffers[ShaderType][BufferIndex] = va;
			ConstantBufferObjects[ShaderType][BufferIndex] = UniformBuffer;
			m_GraphicsBindDirtyMask |= kGraphicsDirtyCBV;
		}
		if (BufferIndex == 0 && ShaderType < SF_NumStandardFrequencies)
			m_RootConstantUniformBuffer[ShaderType] = UniformBuffer;
	}

	void FD3D12StateCache::SetShaderResourceView(EShaderFrequency ShaderType, uint32_t TextureIndex, std::shared_ptr<D3D12Texture2D> Texture2D)
	{
		Assert(TextureIndex < MAX_SRVS);
		D3D12_CPU_DESCRIPTOR_HANDLE Stored{};
		if (Texture2D)
			Stored = Texture2D->GetSRV();
		else
			Stored.ptr = D3D12_GPU_VIRTUAL_ADDRESS_NULL;
		if (ShaderResourceViewCache.Views[ShaderType][TextureIndex].ptr != Stored.ptr)
		{
			ShaderResourceViewCache.Views[ShaderType][TextureIndex] = Stored;
			MarkSrvSlotDirty(ShaderType, TextureIndex);
		}
	}

	void FD3D12StateCache::SetShaderResourceView(EShaderFrequency ShaderType, uint32_t TextureIndex, int32_t Mip, std::shared_ptr<D3D12TextureCube> TextureCube)
	{
		Assert(TextureIndex < MAX_SRVS);
		if (ShaderResourceViewCache.Views[ShaderType][TextureIndex].ptr != TextureCube->GetCubeSRV(Mip).ptr)
		{
			ShaderResourceViewCache.Views[ShaderType][TextureIndex] = TextureCube->GetCubeSRV(Mip);
			MarkSrvSlotDirty(ShaderType, TextureIndex);
		}
	}

	void FD3D12StateCache::SetShaderResourceView(EShaderFrequency ShaderType, uint32_t SRVIndex, std::shared_ptr<D3D12StructuredBuffer> StructuredBuffer)
	{
		Assert(SRVIndex < MAX_SRVS);
		D3D12_CPU_DESCRIPTOR_HANDLE Stored{};
		if (StructuredBuffer)
			Stored = StructuredBuffer->GetSRV();
		else
			Stored.ptr = D3D12_GPU_VIRTUAL_ADDRESS_NULL;
		if (ShaderResourceViewCache.Views[ShaderType][SRVIndex].ptr != Stored.ptr)
		{
			ShaderResourceViewCache.Views[ShaderType][SRVIndex] = Stored;
			MarkSrvSlotDirty(ShaderType, SRVIndex);
		}
	}

	void FD3D12StateCache::SetUAV(uint32_t TextureIndex, std::shared_ptr<D3D12Texture2D> Texture2D)
	{
		Assert(TextureIndex < MAX_UAVS);
		if (UAVCache.Views[TextureIndex].ptr != Texture2D->GetUAV().ptr)
		{
			UAVCache.Views[TextureIndex] = Texture2D->GetUAV();
			m_GraphicsBindDirtyMask |= kGraphicsDirtySRV;
		}
	}

	void FD3D12StateCache::SetUAV(uint32_t UAVIndex, std::shared_ptr<D3D12StructuredBuffer> StructuredBuffer)
	{
		Assert(UAVIndex < MAX_UAVS);
		// Null binding (clearing) goes through with a null handle so a stale UAV slot doesn't survive across shader swaps.
		const D3D12_CPU_DESCRIPTOR_HANDLE Handle = StructuredBuffer ? StructuredBuffer->GetUAV() : D3D12_CPU_DESCRIPTOR_HANDLE{ D3D12_GPU_VIRTUAL_ADDRESS_NULL };
		if (UAVCache.Views[UAVIndex].ptr != Handle.ptr)
		{
			UAVCache.Views[UAVIndex] = Handle;
			m_GraphicsBindDirtyMask |= kGraphicsDirtySRV;
		}
	}

	void FD3D12StateCache::SetDescriptorHeap(D3D12CommandListHandle& CommandList, D3D12_DESCRIPTOR_HEAP_TYPE Type, win32::com_ptr<ID3D12DescriptorHeap> HeapPtr)
	{
		// Avoid redundant SetDescriptorHeaps when the dynamic ring hands back the same heap pointer.
		// After ID3D12GraphicsCommandList::Reset, heaps are not bound even if our cached pointers match — see InvalidateDescriptorHeapBindingsForFreshCommandList.
		ID3D12GraphicsCommandList* const GfxCmdList = CommandList.GraphicsCommandList();
		const bool bCommandListChanged =
			(GfxCmdList != m_LastDescriptorHeapBoundCmdList) ||
			(CommandList.GetRecordingGeneration() != m_LastDescriptorHeapBoundRecordingGen);
		if (CurrentDescriptorHeaps[Type] == HeapPtr && !bCommandListChanged)
			return;
		CurrentDescriptorHeaps[Type] = HeapPtr;
		BindDescriptorHeaps(CommandList);
		m_LastDescriptorHeapBoundCmdList = GfxCmdList;
		m_LastDescriptorHeapBoundRecordingGen = CommandList.GetRecordingGeneration();
	}

	void FD3D12StateCache::BindDescriptorHeaps(D3D12CommandListHandle& CommandList)
	{
		ID3D12GraphicsCommandList* GfxCmdList = CommandList.GraphicsCommandList();
		if (!GfxCmdList)
			return;
		uint32_t NonNullHeaps = 0;
		ID3D12DescriptorHeap* HeapsToBind[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES]{};
		for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
		{
			if (CurrentDescriptorHeaps[i] != nullptr)
			{
				HeapsToBind[NonNullHeaps++] = CurrentDescriptorHeaps[i].get();
			}
		}

		if (NonNullHeaps > 0)
		{
			GfxCmdList->SetDescriptorHeaps(NonNullHeaps, HeapsToBind);
		}
	}

	void FD3D12StateCache::ResetGraphicsApplyTracking()
	{
		m_LastAppliedGraphicsPSOHash = (size_t)-1;
		m_LastAppliedGraphicsRootSig = nullptr;
		m_LastAppliedGraphicsPSO.reset();
		m_LastAppliedGraphicsCmdList = nullptr;
		m_LastAppliedGraphicsRecordingGen = 0;
		m_GraphicsBindDirtyMask = 0;
		m_GraphicsLayoutDirty = true;
		ResetSrvDirtyTracking();
		m_LastUnifiedRootCacheKey.clear();
		for (int32_t fi = 0; fi < SF_NumStandardFrequencies; ++fi)
		{
			for (int32_t bi = 0; bi < MAX_CBS; ++bi)
			{
				if (ConstantBufferObjects[fi][bi])
					ConstantBufferObjects[fi][bi]->ResetGpuRingFences();
				ConstantBufferObjects[fi][bi].reset();
			}
		}
		for (std::shared_ptr<D3D12UniformBuffer>& Ptr : m_RootConstantUniformBuffer)
		{
			if (Ptr)
				Ptr->ResetGpuRingFences();
			Ptr.reset();
		}
	}

	void FD3D12StateCache::SetRenderTargetFormats(const std::vector<std::shared_ptr<RHITexture2D>>& Targets, std::shared_ptr< RHITexture2D> Depth)
	{
		DXGI_FORMAT OldRtv[MaxSimultaneousRenderTargets];
		std::memcpy(OldRtv, PSDesc.RTVFormats, sizeof(OldRtv));
		const uint32_t OldNum = PSDesc.NumRenderTargets;
		const DXGI_FORMAT OldDsv = PSDesc.DSVFormat;

		for (uint32_t i = 0; i < (uint32_t)Targets.size(); ++i)
		{
			const std::shared_ptr<RHITexture2D>& T = Targets[i];
			D3D12Texture2D* Tex2D = T ? RHIResourceCast(T.get()) : nullptr;
			if (!Tex2D)
			{
				static bool sLoggedOnce = false;
				if (!sLoggedOnce)
				{
					sLoggedOnce = true;
					core::LOG(core::log_err, L"[D3D12] SetRenderTargetFormats: null render target at index %u (forcing DXGI_FORMAT_UNKNOWN)", (unsigned)i);
				}
				PSDesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
				continue;
			}
			PSDesc.RTVFormats[i] = Tex2D->GetPlatformResourceFormat();
		}
		for (uint32_t i = (uint32_t)Targets.size(); i < MaxSimultaneousRenderTargets; ++i)
			PSDesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
		PSDesc.NumRenderTargets = (uint32_t)Targets.size();
		D3D12Texture2D* DepthTex = Depth ? RHIResourceCast(Depth.get()) : nullptr;
		const bool bDepthBindable = DepthTex && DepthTex->GetDSV().ptr != 0u;
		PSDesc.DSVFormat = bDepthBindable ? GetPSODepthStencilFormatFromResourceFormat(DepthTex->GetPlatformResourceFormat()) : DXGI_FORMAT_UNKNOWN;
		PSDesc.SampleDesc.Count = 1;
		PSDesc.SampleDesc.Quality = 0;

		if (OldNum != PSDesc.NumRenderTargets || OldDsv != PSDesc.DSVFormat
			|| std::memcmp(OldRtv, PSDesc.RTVFormats, sizeof(OldRtv)) != 0)
			MarkGraphicsLayoutDirty();
	}

	void FD3D12StateCache::SetRenderTargetFormat(const D3D12RenderTarget* RenderTarget)
	{
		DXGI_FORMAT OldRtv[MaxSimultaneousRenderTargets];
		std::memcpy(OldRtv, PSDesc.RTVFormats, sizeof(OldRtv));
		const uint32_t OldNum = PSDesc.NumRenderTargets;
		const DXGI_FORMAT OldDsv = PSDesc.DSVFormat;

		const bool bHasColorTex = RenderTarget && RenderTarget->GetTex() && RenderTarget->GetMipRTV(0).ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL;
		if (bHasColorTex)
		{
			D3D12Texture2D* Tex2D = RHIResourceCast(RenderTarget->GetTex().get());
			PSDesc.RTVFormats[0] = Tex2D ? Tex2D->GetPlatformResourceFormat() : DXGI_FORMAT_UNKNOWN;
			for (uint32_t i = 1; i < MaxSimultaneousRenderTargets; ++i)
				PSDesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
			PSDesc.NumRenderTargets = 1;
		}
		else
		{
			for (uint32_t i = 0; i < MaxSimultaneousRenderTargets; ++i)
				PSDesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
			PSDesc.NumRenderTargets = 0;
		}

		if (RenderTarget && RenderTarget->GetDepthResource() && RenderTarget->GetDSV().ptr != 0u)
			PSDesc.DSVFormat = GetPSODepthStencilFormatFromResourceFormat(RenderTarget->GetDepthResource()->GetDesc().Format);
		else
			PSDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
		PSDesc.SampleDesc.Count = 1;
		PSDesc.SampleDesc.Quality = 0;

		if (OldNum != PSDesc.NumRenderTargets || OldDsv != PSDesc.DSVFormat
			|| std::memcmp(OldRtv, PSDesc.RTVFormats, sizeof(OldRtv)) != 0)
			MarkGraphicsLayoutDirty();
	}

	void FD3D12StateCache::SetRenderTargetFormat(const D3D12TextureCube* RenderTarget)
	{
		DXGI_FORMAT OldRtv[MaxSimultaneousRenderTargets];
		std::memcpy(OldRtv, PSDesc.RTVFormats, sizeof(OldRtv));
		const uint32_t OldNum = PSDesc.NumRenderTargets;
		const DXGI_FORMAT OldDsv = PSDesc.DSVFormat;

		if (RenderTarget->IsShadowDepthCube())
		{
			for (uint32_t i = 0; i < MaxSimultaneousRenderTargets; ++i)
				PSDesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
			PSDesc.NumRenderTargets = 0;
			if (RenderTarget->GetDepthResource())
				PSDesc.DSVFormat = GetPSODepthStencilFormatFromResourceFormat(RenderTarget->GetDepthResource()->GetDesc().Format);
			else
				PSDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
		}
		else
		{
			PSDesc.RTVFormats[0] = RenderTarget->GetPlatformResourceFormat();
			for (uint32_t i = 1; i < MaxSimultaneousRenderTargets; ++i)
				PSDesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
			PSDesc.NumRenderTargets = 1;
			if (RenderTarget->GetDepthResource() && RenderTarget->GetDSV().ptr != 0u)
				PSDesc.DSVFormat = GetPSODepthStencilFormatFromResourceFormat(RenderTarget->GetDepthResource()->GetDesc().Format);
			else
				PSDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
		}
		PSDesc.SampleDesc.Count = 1;
		PSDesc.SampleDesc.Quality = 0;

		if (OldNum != PSDesc.NumRenderTargets || OldDsv != PSDesc.DSVFormat
			|| std::memcmp(OldRtv, PSDesc.RTVFormats, sizeof(OldRtv)) != 0)
			MarkGraphicsLayoutDirty();
	}

	void FD3D12StateCache::SetIndexBuffer(D3D12CommandListHandle& CommandList, const D3D12_INDEX_BUFFER_VIEW& View)
	{
		CommandList->IASetIndexBuffer(&View);
	}

	void FD3D12StateCache::SetVertexBuffer(D3D12CommandListHandle& CommandList,
										   uint32_t Slot, const D3D12_VERTEX_BUFFER_VIEW& View)
	{
		SetVertexBuffers(CommandList,Slot, 1, &View);
	}

	void FD3D12StateCache::SetVertexBuffers(D3D12CommandListHandle& CommandList,
											uint32_t StartSlot, uint32_t Count,
											const D3D12_VERTEX_BUFFER_VIEW View[])
	{
		CommandList->IASetVertexBuffers(StartSlot, Count, View);
	}

	std::shared_ptr<FRootSignature> FD3D12StateCache::BuildRootSignature()
	{
		std::shared_ptr<FD3D12VertexShader> VertexShader;
		FShaderCodePackedResourceCounts VertexResCount{};
		std::string KeyName;
		auto itFindVertexShader = VertexShaders.find(CurrentVertexHash);
		if (itFindVertexShader != VertexShaders.end())
		{
			VertexShader = itFindVertexShader->second;
			VertexResCount = VertexShader->ResourceCounts;
			KeyName = itFindVertexShader->second->KeyName;
		}

		std::shared_ptr<FD3D12PixelShader> PixelShader;
		FShaderCodePackedResourceCounts PixelResCount{};
		auto itFindPixelShader = PixelShaders.find(CurrentPixelHash);
		if (itFindPixelShader != PixelShaders.end())
		{
			PixelShader = itFindPixelShader->second;
			PixelResCount = PixelShader->ResourceCounts;
			KeyName += itFindPixelShader->second->PSEntryPoint;
		}

		std::shared_ptr<FD3D12ComputeShader> ComputeShader;
		FShaderCodePackedResourceCounts ComputeResCount{};
		auto itFindComputeShader = ComputeShaders.find(CurrentComputeHash);
		if (itFindComputeShader != ComputeShaders.end())
		{
			ComputeShader = itFindComputeShader->second;
			ComputeResCount = ComputeShader->ResourceCounts;
			KeyName += itFindComputeShader->second->CSEntryPoint;
		}

		AppendStaticSamplerDigestToRootCacheKey(KeyName, VertexResCount.NumSamplers, PixelResCount.NumSamplers, ComputeResCount.NumSamplers, SamplerCache);

		// FD3D12Shader KeyName is only "FileName_Entry"; macros / vertex-decl variants (skinning, tangent,
		// RHI_BINDLESS vs not, etc.) fold into stable-ish Hash bytes but previously did NOT fold into the
		// root-signature cache key. Re-using the first-seen layout for another variant ⇒ root/bytecode
		// mismatch and CreateGraphicsPipelineState E_INVALIDARG (often misread as root creation failure).
		KeyName += "_vh";
		KeyName += std::to_string(static_cast<unsigned long long>(CurrentVertexHash));
		KeyName += "_ph";
		KeyName += std::to_string(static_cast<unsigned long long>(CurrentPixelHash));
		KeyName += "_ch";
		KeyName += std::to_string(static_cast<unsigned long long>(CurrentComputeHash));

		m_LastUnifiedRootCacheKey = KeyName;

		{
			std::lock_guard<std::recursive_mutex> RootPsoLock(m_RootSignatureAndPsoCacheMutex);
			auto ItEarly = RootSignatures.find(KeyName);
			if (ItEarly != RootSignatures.end())
				return ItEarly->second;
		}

		auto RootSignature = std::make_shared<FRootSignature>(GetParentDevice());

		int32_t NumRootParams = 0;
		NumRootParams += VertexResCount.NumCBs;
		NumRootParams += PixelResCount.NumCBs;
		NumRootParams += ComputeResCount.NumCBs;
		if (VertexResCount.NumSRVs > 0)
			NumRootParams += 1;
		if (PixelResCount.NumSRVs > 0)
			NumRootParams += 1;
		if (PixelResCount.NumUAVs > 0)
			NumRootParams += 1;
		if (ComputeResCount.NumSRVs > 0)
			NumRootParams += 1;
		if (ComputeResCount.NumUAVs > 0)
			NumRootParams += 1;

		RootSignature->Reset(NumRootParams, VertexResCount.NumSamplers + PixelResCount.NumSamplers + ComputeResCount.NumSamplers);

		int32_t RootIndex = 0;
		if (VertexResCount.NumCBs > 0)
		{
			const uint8_t vsRootDw = VertexShader ? VertexShader->CBBind0RootConstantsDwords : 0u;
			if (vsRootDw > 0)
			{
				RootSignature->RootConstantsRootIndex[SF_Vertex] = RootIndex;
				RootSignature->RootConstantsNum32BitValues[SF_Vertex] = vsRootDw;
				(*RootSignature)[RootIndex].InitAsConstants(0, vsRootDw, D3D12_SHADER_VISIBILITY_VERTEX);
				++RootIndex;
				if (VertexResCount.NumCBs > 1)
				{
					RootSignature->CBRootIndex[SF_Vertex] = RootIndex;
					for (int32_t cbReg = 1; cbReg < VertexResCount.NumCBs; ++cbReg)
					{
						(*RootSignature)[RootIndex].InitAsBufferCBV((UINT)cbReg, D3D12_SHADER_VISIBILITY_VERTEX);
						++RootIndex;
					}
				}
			}
			else
			{
				RootSignature->CBRootIndex[SF_Vertex] = RootIndex;
				for (int32_t index = 0; index < VertexResCount.NumCBs; ++index)
				{
					(*RootSignature)[RootIndex].InitAsBufferCBV(index, D3D12_SHADER_VISIBILITY_VERTEX);
					++RootIndex;
				}
			}
		}

		// VS SRV table (e.g. bone SRV / structured buffer); PS/CS tables existed separately.
		if (VertexResCount.NumSRVs > 0)
		{
			(*RootSignature)[RootIndex].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, VertexResCount.NumSRVs, D3D12_SHADER_VISIBILITY_VERTEX);
			RootSignature->SRVRootIndex[SF_Vertex] = RootIndex;
			++RootIndex;
		}

		if (PixelResCount.NumCBs > 0)
		{
			const uint8_t psRootDw = PixelShader ? PixelShader->CBBind0RootConstantsDwords : 0u;
			if (psRootDw > 0)
			{
				RootSignature->RootConstantsRootIndex[SF_Pixel] = RootIndex;
				RootSignature->RootConstantsNum32BitValues[SF_Pixel] = psRootDw;
				(*RootSignature)[RootIndex].InitAsConstants(0, psRootDw, D3D12_SHADER_VISIBILITY_PIXEL);
				++RootIndex;
				if (PixelResCount.NumCBs > 1)
				{
					RootSignature->CBRootIndex[SF_Pixel] = RootIndex;
					for (int32_t cbReg = 1; cbReg < PixelResCount.NumCBs; ++cbReg)
					{
						(*RootSignature)[RootIndex].InitAsBufferCBV((UINT)cbReg, D3D12_SHADER_VISIBILITY_PIXEL);
						++RootIndex;
					}
				}
			}
			else
			{
				RootSignature->CBRootIndex[SF_Pixel] = RootIndex;
				for (int32_t index = 0; index < PixelResCount.NumCBs; ++index)
				{
					(*RootSignature)[RootIndex].InitAsBufferCBV(index, D3D12_SHADER_VISIBILITY_PIXEL);
					++RootIndex;
				}
			}
		}

		if (ComputeResCount.NumCBs > 0)
		{
			const uint8_t csRootDw = ComputeShader ? ComputeShader->CBBind0RootConstantsDwords : 0u;
			if (csRootDw > 0)
			{
				RootSignature->RootConstantsRootIndex[SF_Compute] = RootIndex;
				RootSignature->RootConstantsNum32BitValues[SF_Compute] = csRootDw;
				(*RootSignature)[RootIndex].InitAsConstants(0, csRootDw, D3D12_SHADER_VISIBILITY_ALL);
				++RootIndex;
				if (ComputeResCount.NumCBs > 1)
				{
					RootSignature->CBRootIndex[SF_Compute] = RootIndex;
					for (int32_t cbReg = 1; cbReg < ComputeResCount.NumCBs; ++cbReg)
					{
						(*RootSignature)[RootIndex].InitAsBufferCBV((UINT)cbReg, D3D12_SHADER_VISIBILITY_ALL);
						++RootIndex;
					}
				}
			}
			else
			{
				RootSignature->CBRootIndex[SF_Compute] = RootIndex;
				for (int32_t index = 0; index < ComputeResCount.NumCBs; ++index)
				{
					(*RootSignature)[RootIndex].InitAsBufferCBV(index, D3D12_SHADER_VISIBILITY_ALL);
					++RootIndex;
				}
			}
		}

		if (PixelResCount.NumSRVs > 0)
		{
			(*RootSignature)[RootIndex].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, PixelResCount.NumSRVs, D3D12_SHADER_VISIBILITY_PIXEL);
			RootSignature->SRVRootIndex[SF_Pixel] = RootIndex;
			++RootIndex;
		}

		if (PixelResCount.NumUAVs > 0)
		{
			(*RootSignature)[RootIndex].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, PixelResCount.NumUAVs);
			RootSignature->UAVRootIndex[SF_Compute] = RootIndex;
			++RootIndex;
		}

		if (ComputeResCount.NumSRVs > 0)
		{
			(*RootSignature)[RootIndex].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, ComputeResCount.NumSRVs);
			RootSignature->SRVRootIndex[SF_Compute] = RootIndex;
			++RootIndex;
		}

		if (ComputeResCount.NumUAVs > 0)
		{
			(*RootSignature)[RootIndex].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, ComputeResCount.NumUAVs);
			RootSignature->UAVRootIndex[SF_Compute] = RootIndex;
			++RootIndex;
		}

		if (VertexResCount.NumSamplers > 0)
		{
			auto& Samplers = SamplerCache.States[SF_Vertex];
			for (uint32_t index = 0; index < VertexResCount.NumSamplers; ++index)
				RootSignature->InitStaticSampler(index, Samplers[index], D3D12_SHADER_VISIBILITY_VERTEX);
		}

		if (PixelResCount.NumSamplers > 0)
		{
			auto& Samplers = SamplerCache.States[SF_Pixel];
			for (uint32_t index = 0; index < PixelResCount.NumSamplers; ++index)
				RootSignature->InitStaticSampler(index, Samplers[index], D3D12_SHADER_VISIBILITY_PIXEL);
		}

		if (ComputeResCount.NumSamplers > 0)
		{
			auto& Samplers = SamplerCache.States[SF_Compute];
			for (uint32_t index = 0; index < ComputeResCount.NumSamplers; ++index)
				RootSignature->InitStaticSampler(index, Samplers[index], D3D12_SHADER_VISIBILITY_ALL);
		}

		{
			std::lock_guard<std::recursive_mutex> RootPsoLock(m_RootSignatureAndPsoCacheMutex);
			auto ItMid = RootSignatures.find(KeyName);
			if (ItMid != RootSignatures.end())
				return ItMid->second;
		}

		const std::wstring RootNameW = core::ansi_ucs2(KeyName);
		bool FinalOk = false;
		ENQUEUE_RHI_COMMAND(RootSignatureFinalize,
			FinalOk = RootSignature->Finalize(RootNameW, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
		);

		std::lock_guard<std::recursive_mutex> RootPsoLock(m_RootSignatureAndPsoCacheMutex);
		auto ItRootSignature = RootSignatures.find(KeyName);
		if (ItRootSignature != RootSignatures.end())
			return ItRootSignature->second;
		if (!FinalOk)
			return {};
		RootSignatures.insert({ KeyName, RootSignature });
		return RootSignature;
	}

	void FD3D12StateCache::EnsurePSODepthFormatMatchesBoundDepth(bool bDepthBoundOnOM)
	{
		if (bDepthBoundOnOM)
			return;

		bool bDirty = false;
		if (PSDesc.DSVFormat != DXGI_FORMAT_UNKNOWN)
		{
			PSDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
			bDirty = true;
		}
		// Depth test on + no DSV in the PSO layout triggers D3D12 #680 (and is undefined on some paths).
		if (PSDesc.DepthStencilState.DepthEnable || PSDesc.DepthStencilState.StencilEnable)
		{
			ApplyDisabledDepthStencilToPSDesc(PSDesc.DepthStencilState);
			bDirty = true;
		}
		if (bDirty)
			MarkGraphicsLayoutDirty();
	}

	bool FD3D12StateCache::ApplyGraphicState(D3D12CommandListHandle& CommandList)
	{
		const bool bCmdListChanged =
			(CommandList.GraphicsCommandList() != m_LastAppliedGraphicsCmdList) ||
			(CommandList.GetRecordingGeneration() != m_LastAppliedGraphicsRecordingGen);

		// UE-style: ensure PSO depth state is consistent with OM bindings before creating/applying PSO.
		// When OM has no DSV bound, the PSO must not reference a depth format (D3D12 #615/#680).
		// Replaces legacy per-draw OM rebinding (stale PrepareForGraphicsDraw / cached OM declarations removed from D3D12CommandContext).
		EnsurePSODepthFormatMatchesBoundDepth(m_bDepthBoundOnOMForPso);

		auto RootSignature = BuildRootSignature();
		if (!RootSignature)
		{
			LogApplyGraphicStateFailedOnce("no root signature");
			return false;
		}

		PSDesc.pRootSignature = RootSignature->GetSignature();
		Assert(PSDesc.pRootSignature != nullptr);
		void* const RootSigPtr = reinterpret_cast<void*>(PSDesc.pRootSignature);

		auto itVertexShader = VertexShaders.find(CurrentVertexHash);
		if (itVertexShader == VertexShaders.end())
		{
			LogApplyGraphicStateFailedOnce("vertex shader not in cache");
			Assert(false);
			return false;
		}

		auto itPixelShader = PixelShaders.find(CurrentPixelHash);
		if (itPixelShader == PixelShaders.end())
		{
			LogApplyGraphicStateFailedOnce("pixel shader not in cache");
			Assert(false);
			return false;
		}

		FShaderCodePackedResourceCounts VertexResCount = itVertexShader->second->ResourceCounts;
		FShaderCodePackedResourceCounts PixelResCount = itPixelShader->second->ResourceCounts;

		if (bNeedSetBlendFactor)
		{
			CommandList->OMSetBlendFactor(CurrentBlendFactor);
			bNeedSetBlendFactor = false;
		}
		if (bNeedSetStencilRef)
		{
			CommandList->OMSetStencilRef(CurrentReferenceStencil);
			bNeedSetStencilRef = false;
		}

		if (bNeedSetPrimitiveTopology)
		{
			CommandList->IASetPrimitiveTopology(CurrentPrimitiveTopology);
			bNeedSetPrimitiveTopology = false;
		}

		auto& ElementDescs = itVertexShader->second->ElementDescs;
		InputLayouts.resize(ElementDescs.size());

		int32_t Index = 0;
		for (const auto& Item : ElementDescs)
		{
			D3D12_INPUT_ELEMENT_DESC& ElementDesc = InputLayouts[Index++];
			ElementDesc.SemanticName = Item.SemanticName;
			ElementDesc.SemanticIndex = Item.SemanticIndex;
			ElementDesc.Format = static_cast<DXGI_FORMAT>(Item.Format);
			ElementDesc.InputSlot = Item.InputSlot;
			ElementDesc.AlignedByteOffset = Item.AlignedByteOffset;
			ElementDesc.InputSlotClass = static_cast<D3D12_INPUT_CLASSIFICATION>(Item.InputSlotClass);
			ElementDesc.InstanceDataStepRate = Item.InstanceDataStepRate;
		}
		PSDesc.NodeMask = 1;
		PSDesc.SampleMask = (UINT)-1;
		PSDesc.InputLayout.NumElements = (uint32_t)ElementDescs.size();
		if (PSDesc.InputLayout.NumElements > 0)
			PSDesc.InputLayout.pInputElementDescs = InputLayouts.data();
		else
			PSDesc.InputLayout.pInputElementDescs = nullptr;

		// Stable hash: include root layout key (samplers + b0 root-constants vs CBV) so PSO matches the root signature it was created with.
		const size_t HashCode = HashGraphicsPSOStable(PSDesc, CurrentVertexHash, CurrentPixelHash, ElementDescs, m_LastUnifiedRootCacheKey);

		const bool bLayoutDirty = m_GraphicsLayoutDirty
			|| bCmdListChanged
			|| HashCode != m_LastAppliedGraphicsPSOHash
			|| RootSigPtr != m_LastAppliedGraphicsRootSig;

		if (!bLayoutDirty && m_GraphicsBindDirtyMask == 0)
			return true;

		win32::com_ptr<ID3D12PipelineState> PipelineState;
		const std::shared_ptr<FD3D12Device> ParentDeviceStrong = GetParentDevice();
		ID3D12Device* const D3DDevice = ParentDeviceStrong ? ParentDeviceStrong->GetDevice() : nullptr;
		bool bHaveGraphicsPso = false;
		{
			std::lock_guard<std::recursive_mutex> PsoLock(m_RootSignatureAndPsoCacheMutex);
			auto iter = GraphicsPSHashMap.find(HashCode);
			if (iter != GraphicsPSHashMap.end())
			{
				PipelineState = iter->second;
				bHaveGraphicsPso = true;
			}
		}
		if (!bHaveGraphicsPso)
		{
			if (!D3DDevice)
			{
				LogApplyGraphicStateFailedOnce("null D3D12 device");
				return false;
			}
			D3D12_GRAPHICS_PIPELINE_STATE_DESC DescCopy = PSDesc;
			win32::com_ptr<ID3D12PipelineState> Created;
			HRESULT Hr = S_OK;
			ENQUEUE_RHI_COMMAND(CreateGraphicsPSO,
				Hr = D3DDevice->CreateGraphicsPipelineState(&DescCopy, IID_PPV_ARGS(Created.get_init_ref()));
			);
			if (FAILED(Hr))
			{
				static std::atomic<int> s_psoFailDetailLogsRemaining{16};
				const int left = s_psoFailDetailLogsRemaining.fetch_sub(1);
				if (left > 0)
				{
					core::err() << "CreateGraphicsPipelineState failed hr=0x" << std::hex << std::uppercase << static_cast<unsigned long>(Hr)
								<< " (VS/PS I/O mismatch vs optimized .cso often means shaderdebug=1 - disable or pass shaderjit for all-JIT)";
				}
				else if (left == 0)
				{
					core::err() << "[D3D12] CreateGraphicsPipelineState: further duplicate failure detail logs suppressed";
				}
				LogApplyGraphicStateFailedOnce("CreateGraphicsPipelineState failed");
				Assert(false);
				return false;
			}
			std::lock_guard<std::recursive_mutex> PsoLock(m_RootSignatureAndPsoCacheMutex);
			auto iter2 = GraphicsPSHashMap.find(HashCode);
			if (iter2 != GraphicsPSHashMap.end())
				PipelineState = iter2->second;
			else
			{
				PipelineState = Created;
				GraphicsPSHashMap[HashCode] = PipelineState;
			}
		}

		auto bindAllGraphicsCbvs = [&]()
		{
			auto Dev = GetParentDevice();
			auto NullUB = Dev ? Dev->GetNullUniformBuffer() : nullptr;
			auto pushRootConstants = [&](EShaderFrequency Freq)
			{
				const int32_t rcIdx = RootSignature->RootConstantsRootIndex[Freq];
				if (rcIdx < 0)
					return;
				const UINT N = (UINT)RootSignature->RootConstantsNum32BitValues[Freq];
				if (N == 0)
					return;
				std::shared_ptr<D3D12UniformBuffer> ub = m_RootConstantUniformBuffer[Freq];
				if (ub)
				{
					CommandList.SetGraphicsRoot32BitConstantsFromUniform((UINT)rcIdx, N, ub, 0);
				}
				else
				{
					// If the app didn't provide constants, explicitly set zeros.
					std::vector<uint32_t> Zero;
					Zero.resize(N, 0u);
					CommandList.GraphicsCommandList()->SetGraphicsRoot32BitConstants((UINT)rcIdx, N, Zero.data(), 0);
				}
			};
			pushRootConstants(SF_Vertex);
			pushRootConstants(SF_Pixel);

			int32_t StartIndex = RootSignature->CBRootIndex[SF_Vertex];
			if (StartIndex >= 0)
			{
				const uint32_t firstReg = (RootSignature->RootConstantsRootIndex[SF_Vertex] >= 0) ? 1u : 0u;
				uint32_t rootParamOffset = 0;
				for (uint32_t reg = firstReg; reg < VertexResCount.NumCBs; ++reg)
				{
					std::shared_ptr<D3D12UniformBuffer> ub = ConstantBufferObjects[SF_Vertex][reg];
					if (!ub)
						ub = NullUB;
					if (ub)
					{
						CommandList.SetGraphicsRootConstantBufferViewUniform(
							rootParamOffset + StartIndex,
							ub);
					}
					++rootParamOffset;
				}
			}
			StartIndex = RootSignature->CBRootIndex[SF_Pixel];
			if (StartIndex >= 0)
			{
				const uint32_t firstReg = (RootSignature->RootConstantsRootIndex[SF_Pixel] >= 0) ? 1u : 0u;
				uint32_t rootParamOffset = 0;
				for (uint32_t reg = firstReg; reg < PixelResCount.NumCBs; ++reg)
				{
					std::shared_ptr<D3D12UniformBuffer> ub = ConstantBufferObjects[SF_Pixel][reg];
					if (!ub)
						ub = NullUB;
					if (ub)
					{
						CommandList.SetGraphicsRootConstantBufferViewUniform(
							rootParamOffset + StartIndex,
							ub);
					}
					++rootParamOffset;
				}
			}
		};

		auto stageGraphicsSrvsForFrequency = [&](EShaderFrequency Freq, uint32_t NumSrvs, const uint8_t* SlotNullDims, bool bFullTable)
		{
			const int32_t rootParamIndex = RootSignature->SRVRootIndex[Freq];
			if (rootParamIndex < 0 || NumSrvs == 0)
				return;
			auto DevSrv = GetParentDevice();
			const D3D12_CPU_DESCRIPTOR_HANDLE NullSrv2D = DevSrv ? DevSrv->GetNullSrvCpu() : D3D12_CPU_DESCRIPTOR_HANDLE{ D3D12_GPU_VIRTUAL_ADDRESS_NULL };
			const D3D12_CPU_DESCRIPTOR_HANDLE NullSrvCube = DevSrv ? DevSrv->GetNullSrvCubeCpu() : D3D12_CPU_DESCRIPTOR_HANDLE{ D3D12_GPU_VIRTUAL_ADDRESS_NULL };
			const D3D12_CPU_DESCRIPTOR_HANDLE* views = ShaderResourceViewCache.Views[Freq];
			if (bFullTable || m_SrvDirtyFull[Freq] || m_SrvDirtyMin[Freq] > m_SrvDirtyMax[Freq])
			{
				StageAllGraphicsSrvs(DynamicViewDescriptorHeap, rootParamIndex, NumSrvs, views, NullSrv2D, NullSrvCube, SlotNullDims);
				return;
			}
			uint32_t dirtyMin = m_SrvDirtyMin[Freq];
			uint32_t dirtyMax = m_SrvDirtyMax[Freq];
			if (dirtyMax >= NumSrvs)
				dirtyMax = NumSrvs - 1u;
			StageGraphicsSrvRange(DynamicViewDescriptorHeap, rootParamIndex, dirtyMin, dirtyMax, views, NullSrv2D, NullSrvCube, SlotNullDims);
		};

		if (bLayoutDirty)
		{
			CommandList->SetGraphicsRootSignature(PSDesc.pRootSignature);
			DynamicViewDescriptorHeap.ParseGraphicsRootSignature(*RootSignature);
			CommandList->SetPipelineState(PipelineState.get());
			bindAllGraphicsCbvs();
			stageGraphicsSrvsForFrequency(SF_Vertex, VertexResCount.NumSRVs, VertexResCount.SrvSlotNullViewDimension, true);
			stageGraphicsSrvsForFrequency(SF_Pixel, PixelResCount.NumSRVs, PixelResCount.SrvSlotNullViewDimension, true);
			DynamicViewDescriptorHeap.CommitGraphicsRootDescriptorTables(CommandList.GraphicsCommandList());
			m_LastAppliedGraphicsPSOHash = HashCode;
			m_LastAppliedGraphicsRootSig = RootSigPtr;
			m_LastAppliedGraphicsPSO = PipelineState;
			m_LastAppliedGraphicsCmdList = CommandList.GraphicsCommandList();
			m_LastAppliedGraphicsRecordingGen = CommandList.GetRecordingGeneration();
			m_GraphicsLayoutDirty = false;
			m_GraphicsBindDirtyMask = 0;
			ResetSrvDirtyTracking();
		}
		else
		{
			if (m_GraphicsBindDirtyMask & kGraphicsDirtyCBV)
				bindAllGraphicsCbvs();
			if (m_GraphicsBindDirtyMask & kGraphicsDirtySRV)
			{
				stageGraphicsSrvsForFrequency(SF_Vertex, VertexResCount.NumSRVs, VertexResCount.SrvSlotNullViewDimension, false);
				stageGraphicsSrvsForFrequency(SF_Pixel, PixelResCount.NumSRVs, PixelResCount.SrvSlotNullViewDimension, false);
				DynamicViewDescriptorHeap.CommitGraphicsRootDescriptorTables(CommandList.GraphicsCommandList());
				ResetSrvDirtyTracking();
			}
			m_GraphicsBindDirtyMask = 0;
		}

		return true;
	}

	bool FD3D12StateCache::ApplyComputeState(D3D12CommandListHandle& CommandList)
	{
		const bool bCmdListChanged =
			(CommandList.GraphicsCommandList() != m_LastAppliedComputeCmdList) ||
			(CommandList.GetRecordingGeneration() != m_LastAppliedComputeRecordingGen);
		auto RootSignature = BuildRootSignature();
		if (!RootSignature)
			return false;

		CSDesc.pRootSignature = RootSignature->GetSignature();
		Assert(CSDesc.pRootSignature != nullptr);
		void* const RootSigPtr = reinterpret_cast<void*>(CSDesc.pRootSignature);
		CSDesc.NodeMask = 1;

		auto itComputeShader = ComputeShaders.find(CurrentComputeHash);
		if (itComputeShader == ComputeShaders.end())
		{
			Assert(false);
			return false;
		}

		size_t HashCode = HashComputePSOStable(CSDesc, CurrentComputeHash, m_LastUnifiedRootCacheKey);

		win32::com_ptr<ID3D12PipelineState> PipelineState;
		const std::shared_ptr<FD3D12Device> ParentDeviceStrongCs = GetParentDevice();
		ID3D12Device* const D3DDeviceCs = ParentDeviceStrongCs ? ParentDeviceStrongCs->GetDevice() : nullptr;
		bool bHaveComputePso = false;
		{
			std::lock_guard<std::recursive_mutex> PsoLock(m_RootSignatureAndPsoCacheMutex);
			auto iter = ComputePSHashMap.find(HashCode);
			if (iter != ComputePSHashMap.end())
			{
				PipelineState = iter->second;
				bHaveComputePso = true;
			}
		}
		if (!bHaveComputePso)
		{
			if (!D3DDeviceCs)
				return false;
			D3D12_COMPUTE_PIPELINE_STATE_DESC DescCopy = CSDesc;
			win32::com_ptr<ID3D12PipelineState> Created;
			HRESULT Hr = S_OK;
			ENQUEUE_RHI_COMMAND(CreateComputePSO,
				Hr = D3DDeviceCs->CreateComputePipelineState(&DescCopy, IID_PPV_ARGS(Created.get_init_ref()));
			);
			if (FAILED(Hr))
			{
				Assert(false);
				return false;
			}
			std::lock_guard<std::recursive_mutex> PsoLock(m_RootSignatureAndPsoCacheMutex);
			auto iter2 = ComputePSHashMap.find(HashCode);
			if (iter2 != ComputePSHashMap.end())
				PipelineState = iter2->second;
			else
			{
				PipelineState = Created;
				ComputePSHashMap[HashCode] = PipelineState;
			}
		}
		// A newly Reset command list has no bindings; force full rebind on cmdlist change or root/PSO change.
		const bool bNeedFullBind = bCmdListChanged
			|| RootSigPtr != m_LastAppliedComputeRootSig
			|| PipelineState.get() != m_LastAppliedComputePSO.get();
		if (bNeedFullBind)
		{
			CommandList->SetComputeRootSignature(RootSignature->GetSignature());
			DynamicViewDescriptorHeap.ParseComputeRootSignature(*RootSignature);
			CommandList->SetPipelineState(PipelineState.get());
			m_LastAppliedComputeRootSig = RootSigPtr;
			m_LastAppliedComputePSO = PipelineState;
			m_LastAppliedComputeCmdList = CommandList.GraphicsCommandList();
			m_LastAppliedComputeRecordingGen = CommandList.GetRecordingGeneration();
		}

		FShaderCodePackedResourceCounts ComputeResCount = itComputeShader->second->ResourceCounts;
		auto ParentDev = GetParentDevice();

		const int32_t csRcIdx = RootSignature->RootConstantsRootIndex[SF_Compute];
		if (csRcIdx >= 0)
		{
			const UINT N = (UINT)RootSignature->RootConstantsNum32BitValues[SF_Compute];
			std::shared_ptr<D3D12UniformBuffer> ub = m_RootConstantUniformBuffer[SF_Compute];
			if (ub && N > 0)
			{
				CommandList.SetComputeRoot32BitConstantsFromUniform((UINT)csRcIdx, N, ub, 0);
			}
			else if (N > 0)
			{
				std::vector<uint32_t> Zero;
				Zero.resize(N, 0u);
				CommandList.GraphicsCommandList()->SetComputeRoot32BitConstants((UINT)csRcIdx, N, Zero.data(), 0);
			}
		}

		int32_t StartIndex = RootSignature->CBRootIndex[SF_Compute];
		if (StartIndex >= 0)
		{
			auto NullUB = ParentDev ? ParentDev->GetNullUniformBuffer() : nullptr;
			const uint32_t firstReg = (csRcIdx >= 0) ? 1u : 0u;
			uint32_t rootParamOffset = 0;
			for (uint32_t reg = firstReg; reg < ComputeResCount.NumCBs; ++reg)
			{
				std::shared_ptr<D3D12UniformBuffer> ub = ConstantBufferObjects[SF_Compute][reg];
				if (!ub)
					ub = NullUB;
				if (ub)
				{
					CommandList.SetComputeRootConstantBufferViewUniform(
						rootParamOffset + StartIndex,
						ub);
				}
				++rootParamOffset;
			}
		}

		if (RootSignature->SRVRootIndex[SF_Compute] > -1)
		{
			const D3D12_CPU_DESCRIPTOR_HANDLE NullSrv2D = ParentDev ? ParentDev->GetNullSrvCpu() : D3D12_CPU_DESCRIPTOR_HANDLE{ D3D12_GPU_VIRTUAL_ADDRESS_NULL };
			const D3D12_CPU_DESCRIPTOR_HANDLE NullSrvCube = ParentDev ? ParentDev->GetNullSrvCubeCpu() : D3D12_CPU_DESCRIPTOR_HANDLE{ D3D12_GPU_VIRTUAL_ADDRESS_NULL };
			StageAllComputeSrvs(DynamicViewDescriptorHeap, RootSignature->SRVRootIndex[SF_Compute], ComputeResCount.NumSRVs, ShaderResourceViewCache.Views[SF_Compute],
				NullSrv2D, NullSrvCube, ComputeResCount.SrvSlotNullViewDimension);
		}

		if (RootSignature->UAVRootIndex[SF_Compute] > -1)
		{
			const D3D12_CPU_DESCRIPTOR_HANDLE NullUav = ParentDev ? ParentDev->GetNullUavCpu() : D3D12_CPU_DESCRIPTOR_HANDLE{ D3D12_GPU_VIRTUAL_ADDRESS_NULL };
			StageAllComputeUavs(DynamicViewDescriptorHeap, RootSignature->UAVRootIndex[SF_Compute], ComputeResCount.NumUAVs, UAVCache.Views, NullUav);
		}

		DynamicViewDescriptorHeap.CommitComputeRootDescriptorTables(CommandList.GraphicsCommandList());
		return true;
	}

	void FD3D12StateCache::ClearState()
	{
		m_bDepthBoundOnOMForPso = false;
		// Blend State Cache
		CurrentBlendFactor[0] = D3D12_DEFAULT_BLEND_FACTOR_RED;
		CurrentBlendFactor[1] = D3D12_DEFAULT_BLEND_FACTOR_GREEN;
		CurrentBlendFactor[2] = D3D12_DEFAULT_BLEND_FACTOR_BLUE;
		CurrentBlendFactor[3] = D3D12_DEFAULT_BLEND_FACTOR_ALPHA;

		CurrentReferenceStencil = 0;
		CurrentPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
		CurrentVertexHash = 0;
		CurrentPixelHash = 0;
		CurrentComputeHash = 0;

		PSDesc = {};
		CSDesc = {};
		ConstantBufferCache.Clear();
		SamplerCache.Clear();
		ShaderResourceViewCache.Clear();
		UAVCache.Clear();
		ResetSrvDirtyTracking();

		for (int32_t index = 0; index < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++index)
			CurrentDescriptorHeaps[index] = {};
		m_LastDescriptorHeapBoundCmdList = nullptr;
		m_LastDescriptorHeapBoundRecordingGen = 0;
		ResetGraphicsApplyTracking();
		m_LastAppliedComputeRootSig = nullptr;
		m_LastAppliedComputePSO.reset();
		m_LastAppliedComputeCmdList = nullptr;
	}

	void FD3D12StateCache::ClearRenderState()
	{
		for (int32_t index = 0; index < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++index)
			CurrentDescriptorHeaps[index] = {};
		m_LastDescriptorHeapBoundCmdList = nullptr;
	}

	void FD3D12StateCache::ClearComputeState()
	{
		for (int32_t index = 0; index < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++index)
			CurrentDescriptorHeaps[index] = {};
		m_LastDescriptorHeapBoundCmdList = nullptr;
	}

	void FD3D12StateCache::CleanupUsedHeaps(uint64_t FenceValue, ED3D12CommandQueueType QueueType)
	{
		DynamicViewDescriptorHeap.CleanupUsedHeaps(FenceValue, QueueType);
	}

	void FD3D12StateCache::NotifyExternalGraphicsPassRecorded(D3D12CommandListHandle& CommandList)
	{
		(void)CommandList;
		std::lock_guard<std::recursive_mutex> Lock(m_RootSignatureAndPsoCacheMutex);
		MarkGraphicsLayoutDirty();
		ResetGraphicsApplyTracking();
		m_LastDescriptorHeapBoundCmdList = nullptr;
		m_LastDescriptorHeapBoundRecordingGen = 0;
		for (int32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
			CurrentDescriptorHeaps[i].reset();
		m_LastAppliedComputeRootSig = nullptr;
		m_LastAppliedComputePSO.reset();
		m_LastAppliedComputeCmdList = nullptr;
		m_LastAppliedComputeRecordingGen = 0;
	}

	std::size_t FD3D12StateCache::GetRootSignatureCacheSize() const
	{
		std::lock_guard<std::recursive_mutex> Lock(m_RootSignatureAndPsoCacheMutex);
		return RootSignatures.size();
	}

	std::size_t FD3D12StateCache::GetGraphicsPSOCacheSize() const
	{
		std::lock_guard<std::recursive_mutex> Lock(m_RootSignatureAndPsoCacheMutex);
		return GraphicsPSHashMap.size();
	}

	std::size_t FD3D12StateCache::GetComputePSOCacheSize() const
	{
		std::lock_guard<std::recursive_mutex> Lock(m_RootSignatureAndPsoCacheMutex);
		return ComputePSHashMap.size();
	}

}