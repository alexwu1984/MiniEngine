#include "D3D12/D3D12StateCache.h"
#include "D3D12/D3D12Shaders.h"
#include "common/crc.h"
#include "D3D12/D3D12RootSignature.h"
#include "D3D12/D3D12CommandList.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12UniformBuffer.h"
#include "D3D12/D3D12ReourceTraits.h"
#include "D3D12/D3D12RenderTarget.h"
#include "core/logger.h"

namespace RenderCore
{
	namespace
	{
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
										   const std::vector<VertexElementDesc>& ElementDescs)
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
			return H;
		}

		static size_t HashComputePSOStable(const D3D12_COMPUTE_PIPELINE_STATE_DESC& Desc, uint32_t CSHash)
		{
			size_t H = 0;
			H = HashBytesStable(&CSHash, sizeof(CSHash), H);
			// Exclude pointer fields: pRootSignature and CS bytecode pointers.
			H = HashBytesStable(&Desc.NodeMask, sizeof(Desc.NodeMask), H);
			H = HashBytesStable(&Desc.Flags, sizeof(Desc.Flags), H);
			return H;
		}
	}

	FD3D12StateCache::FD3D12StateCache(std::weak_ptr<FD3D12Device> InParent, std::weak_ptr<D3D12CommandContext> CommandContext)
		:FD3D12DeviceChild(InParent)
		,DynamicViewDescriptorHeap(InParent, CommandContext,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
	{

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
	}

	void FD3D12StateCache::SetPixelShader(std::shared_ptr<FD3D12PixelShader> InPixelShader)
	{
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
	}

	void FD3D12StateCache::SetComputeShader(std::shared_ptr<FD3D12ComputeShader> InComputeShader)
	{
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
		}
		bNeedSetPrimitiveTopology = true;
	}

	void FD3D12StateCache::SetDynamicConstantBuffer(EShaderFrequency ShaderType, uint32_t BufferIndex, std::shared_ptr<D3D12UniformBuffer> UniformBuffer)
	{
		Assert(BufferIndex < MAX_CBS);
		if (ConstantBufferCache.Buffers[ShaderType][BufferIndex] != UniformBuffer->GetGPUVirtualAddress())
		{
			ConstantBufferCache.Buffers[ShaderType][BufferIndex] = UniformBuffer->GetGPUVirtualAddress();
		}
	}

	void FD3D12StateCache::SetShaderResourceView(EShaderFrequency ShaderType, uint32_t TextureIndex, std::shared_ptr<D3D12Texture2D> Texture2D)
	{
		Assert(TextureIndex < MAX_SRVS);
		if (ShaderResourceViewCache.Views[ShaderType][TextureIndex].ptr != Texture2D->GetSRV().ptr)
		{
			ShaderResourceViewCache.Views[ShaderType][TextureIndex] = Texture2D->GetSRV();
		}
	}

	void FD3D12StateCache::SetShaderResourceView(EShaderFrequency ShaderType, uint32_t TextureIndex, int32_t Mip, std::shared_ptr<D3D12TextureCube> TextureCube)
	{
		Assert(TextureIndex < MAX_SRVS);
		if (ShaderResourceViewCache.Views[ShaderType][TextureIndex].ptr != TextureCube->GetCubeSRV(Mip).ptr)
		{
			ShaderResourceViewCache.Views[ShaderType][TextureIndex] = TextureCube->GetCubeSRV(Mip);
		}
	}

	void FD3D12StateCache::SetUAV(uint32_t TextureIndex, std::shared_ptr<D3D12Texture2D> Texture2D)
	{
		Assert(TextureIndex < MAX_UAVS);
		if (UAVCache.Views[TextureIndex].ptr != Texture2D->GetUAV().ptr)
		{
			UAVCache.Views[TextureIndex] = Texture2D->GetUAV();
		}
	}

	void FD3D12StateCache::SetDescriptorHeap(D3D12CommandListHandle& CommandList, D3D12_DESCRIPTOR_HEAP_TYPE Type, win32::com_ptr<ID3D12DescriptorHeap> HeapPtr)
	{
		if (CurrentDescriptorHeaps[Type] != HeapPtr)
			CurrentDescriptorHeaps[Type] = HeapPtr;
		BindDescriptorHeaps(CommandList);
	}

	void FD3D12StateCache::BindDescriptorHeaps(D3D12CommandListHandle& CommandList)
	{
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
			CommandList->SetDescriptorHeaps(NonNullHeaps, HeapsToBind);
		}
	}

	void FD3D12StateCache::SetRenderTargetFormats(const std::vector<std::shared_ptr<RHITexture2D>>& Targets, std::shared_ptr< RHITexture2D> Depth)
	{
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
		PSDesc.DSVFormat = DepthTex ? DepthTex->GetPlatformResourceFormat() : DXGI_FORMAT_UNKNOWN;
		PSDesc.SampleDesc.Count = 1;
		PSDesc.SampleDesc.Quality = 0;
	}

	void FD3D12StateCache::SetRenderTargetFormat(const D3D12RenderTarget* RenderTarget)
	{
		D3D12Texture2D* Tex2D = (RenderTarget && RenderTarget->GetTex()) ? RHIResourceCast(RenderTarget->GetTex().get()) : nullptr;
		if (!Tex2D)
		{
			static bool sLoggedOnce = false;
			if (!sLoggedOnce)
			{
				sLoggedOnce = true;
				core::LOG(core::log_err, L"[D3D12] SetRenderTargetFormat: null render target (forcing DXGI_FORMAT_UNKNOWN)");
			}
			PSDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
		}
		else
		{
			PSDesc.RTVFormats[0] = Tex2D->GetPlatformResourceFormat();
		}
		for (uint32_t i = 1; i < MaxSimultaneousRenderTargets; ++i)
			PSDesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
		PSDesc.NumRenderTargets = 1;
		if (RenderTarget->GetDepthResource())
			PSDesc.DSVFormat = RenderTarget->GetDepthResource()->GetDesc().Format;
		else
			PSDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
		PSDesc.SampleDesc.Count = 1;
		PSDesc.SampleDesc.Quality = 0;
	}

	void FD3D12StateCache::SetRenderTargetFormat(const D3D12TextureCube* RenderTarget)
	{
		PSDesc.RTVFormats[0] = RenderTarget->GetPlatformResourceFormat();
		for (uint32_t i = 1; i < MaxSimultaneousRenderTargets; ++i)
			PSDesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
		PSDesc.NumRenderTargets = 1;
		if (RenderTarget->GetDepthResource())
			PSDesc.DSVFormat = RenderTarget->GetDepthResource()->GetDesc().Format;
		else
			PSDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
		PSDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
		PSDesc.SampleDesc.Count = 1;
		PSDesc.SampleDesc.Quality = 0;
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

		std::shared_ptr<FRootSignature> RootSignature;
		auto ItRootSignature = RootSignatures.find(KeyName);
		if (ItRootSignature != RootSignatures.end())
			return ItRootSignature->second;
		RootSignature = std::make_shared<FRootSignature>(GetParentDevice());

		int32_t NumRootParams = 0;
		NumRootParams += VertexResCount.NumCBs;
		NumRootParams += PixelResCount.NumCBs;
		NumRootParams += ComputeResCount.NumCBs;
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
			RootSignature->CBRootIndex[SF_Vertex] = RootIndex;
			for (int32_t index = 0; index < VertexResCount.NumCBs; ++index)
			{
				(*RootSignature)[RootIndex].InitAsBufferCBV(index, D3D12_SHADER_VISIBILITY_VERTEX);
				++RootIndex;
			}
		}
	
		if (PixelResCount.NumCBs > 0)
		{
			RootSignature->CBRootIndex[SF_Pixel] = RootIndex;
			for (int32_t index = 0; index < PixelResCount.NumCBs; ++index)
			{
				(*RootSignature)[RootIndex].InitAsBufferCBV(index, D3D12_SHADER_VISIBILITY_PIXEL);
				++RootIndex;
			}
		}

		if (ComputeResCount.NumCBs > 0)
		{
			RootSignature->CBRootIndex[SF_Compute] = RootIndex;
			for (int32_t index = 0; index < ComputeResCount.NumCBs; ++index)
			{
				(*RootSignature)[RootIndex].InitAsBufferCBV(index, D3D12_SHADER_VISIBILITY_ALL);
				++RootIndex;
			}
		}

		if (PixelResCount.NumSRVs > 0)
		{
			(*RootSignature)[RootIndex].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, PixelResCount.NumSRVs);
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

		if (RootSignature->Finalize(core::ansi_ucs2(KeyName), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT))
		{
			RootSignatures.insert({ KeyName,RootSignature });
			return RootSignature;
		}
		else
			return {};
	}

	bool FD3D12StateCache::ApplyGraphicState(D3D12CommandListHandle& CommandList)
	{
		auto RootSignature = BuildRootSignature();
		if (!RootSignature)
			return false;

		PSDesc.pRootSignature = RootSignature->GetSignature();
		Assert(PSDesc.pRootSignature != nullptr);

		auto itVertexShader = VertexShaders.find(CurrentVertexHash);
		if (itVertexShader == VertexShaders.end())
		{
			Assert(false);
			return false;
		}

		auto itPixelShader = PixelShaders.find(CurrentPixelHash);
		if (itPixelShader == PixelShaders.end())
		{
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

		auto &ElementDescs  = itVertexShader->second->ElementDescs;
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

		// Use a stable key that doesn't include pointer addresses inside PSDesc (root sig / shader bytecode / input layout).
		size_t HashCode = HashGraphicsPSOStable(PSDesc, CurrentVertexHash, CurrentPixelHash, ElementDescs);

		win32::com_ptr<ID3D12PipelineState> PipelineState;
		{
			auto iter = GraphicsPSHashMap.find(HashCode);
			if (iter == GraphicsPSHashMap.end())
			{
				HRESULT hr = GetParentDevice()->GetDevice()->CreateGraphicsPipelineState(&PSDesc, IID_PPV_ARGS(PipelineState.get_init_ref()));
				if (FAILED(hr))
				{
					Assert(false);
					return false;
				}
				GraphicsPSHashMap[HashCode] = PipelineState;
			}
			else
			{
				PipelineState = GraphicsPSHashMap[HashCode];
			}
		}
		CommandList->SetGraphicsRootSignature(PSDesc.pRootSignature);
		DynamicViewDescriptorHeap.ParseGraphicsRootSignature(*RootSignature);
		CommandList->SetPipelineState(PipelineState.get());

		int32_t StartIndex = RootSignature->CBRootIndex[SF_Vertex];
		for (uint32_t Index = 0; Index < VertexResCount.NumCBs; ++Index)
		{
			if (ConstantBufferCache.Buffers[SF_Vertex][Index] !=  D3D12_GPU_VIRTUAL_ADDRESS_NULL)
			{
				auto Handle = ConstantBufferCache.Buffers[SF_Vertex][Index];
				CommandList->SetGraphicsRootConstantBufferView(Index + StartIndex, Handle);
			}
		}

		StartIndex = RootSignature->CBRootIndex[SF_Pixel];
		for (uint32_t Index = 0; Index < PixelResCount.NumCBs; ++Index)
		{
			if (ConstantBufferCache.Buffers[SF_Pixel][Index] != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
			{
				auto Handle = ConstantBufferCache.Buffers[SF_Pixel][Index];
				CommandList->SetGraphicsRootConstantBufferView(Index + StartIndex, Handle);
			}
		}

		if (RootSignature->SRVRootIndex[SF_Vertex] > -1)
		{
			for (uint32_t Index = 0; Index < VertexResCount.NumSRVs; ++Index)
			{
				if (ShaderResourceViewCache.Views[SF_Vertex][Index].ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
				{
					D3D12_CPU_DESCRIPTOR_HANDLE Handle = ShaderResourceViewCache.Views[SF_Vertex][Index];
					DynamicViewDescriptorHeap.SetGraphicsDescriptorHandles(RootSignature->SRVRootIndex[SF_Vertex], Index, 1, &Handle);
				}
			}
		}

		if (RootSignature->SRVRootIndex[SF_Pixel] > -1)
		{
			for (uint32_t Index = 0; Index < PixelResCount.NumSRVs; ++Index)
			{
				if (ShaderResourceViewCache.Views[SF_Pixel][Index].ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
				{
					D3D12_CPU_DESCRIPTOR_HANDLE Handle = ShaderResourceViewCache.Views[SF_Pixel][Index];
					DynamicViewDescriptorHeap.SetGraphicsDescriptorHandles(RootSignature->SRVRootIndex[SF_Pixel], Index, 1, &Handle);
				}
			}
		}

		DynamicViewDescriptorHeap.CommitGraphicsRootDescriptorTables(CommandList.GraphicsCommandList());
		return true;
	}

	bool FD3D12StateCache::ApplyComputeState(D3D12CommandListHandle& CommandList)
	{
		auto RootSignature = BuildRootSignature();
		if (!RootSignature)
			return false;

		CSDesc.pRootSignature = RootSignature->GetSignature();
		Assert(CSDesc.pRootSignature != nullptr);
		CSDesc.NodeMask = 1;

		auto itComputeShader = ComputeShaders.find(CurrentComputeHash);
		if (itComputeShader == ComputeShaders.end())
		{
			Assert(false);
			return false;
		}

		// Use a stable key that doesn't include pointer addresses inside CSDesc (root sig / shader bytecode).
		size_t HashCode = HashComputePSOStable(CSDesc, CurrentComputeHash);

		win32::com_ptr<ID3D12PipelineState> PipelineState;
		{
			auto iter = ComputePSHashMap.find(HashCode);
			if (iter == ComputePSHashMap.end())
			{
				HRESULT hr = GetParentDevice()->GetDevice()->CreateComputePipelineState(&CSDesc, IID_PPV_ARGS(PipelineState.get_init_ref()));
				if (FAILED(hr))
				{
					Assert(false);
					return false;
				}
				ComputePSHashMap[HashCode] = PipelineState;
			}
			else
			{
				PipelineState = ComputePSHashMap[HashCode];
			}
		}
		CommandList->SetComputeRootSignature(RootSignature->GetSignature());
		DynamicViewDescriptorHeap.ParseComputeRootSignature(*RootSignature);
		CommandList->SetPipelineState(PipelineState.get());

		FShaderCodePackedResourceCounts ComputeResCount = itComputeShader->second->ResourceCounts;

		int32_t StartIndex = RootSignature->CBRootIndex[SF_Compute];
		for (uint32_t Index = 0; Index < ComputeResCount.NumCBs; ++Index)
		{
			if (ConstantBufferCache.Buffers[SF_Compute][Index] != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
			{
				auto Handle = ConstantBufferCache.Buffers[SF_Compute][Index];
				CommandList->SetComputeRootConstantBufferView(Index + StartIndex, Handle);
			}
		}

		if (RootSignature->SRVRootIndex[SF_Compute] > -1)
		{
			for (uint32_t Index = 0; Index < ComputeResCount.NumSRVs; ++Index)
			{
				if (ShaderResourceViewCache.Views[SF_Compute][Index].ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
				{
					D3D12_CPU_DESCRIPTOR_HANDLE Handle = ShaderResourceViewCache.Views[SF_Compute][Index];
					DynamicViewDescriptorHeap.SetComputeDescriptorHandles(RootSignature->SRVRootIndex[SF_Compute], Index, 1, &Handle);
				}
			}
		}

		if (RootSignature->UAVRootIndex[SF_Compute] > -1)
		{
			for (uint32_t Index = 0; Index < ComputeResCount.NumUAVs; ++Index)
			{
				if (UAVCache.Views[Index].ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
				{
					D3D12_CPU_DESCRIPTOR_HANDLE Handle = UAVCache.Views[Index];
					DynamicViewDescriptorHeap.SetComputeDescriptorHandles(RootSignature->UAVRootIndex[SF_Compute], Index, 1, &Handle);
				}
			}
		}

		DynamicViewDescriptorHeap.CommitComputeRootDescriptorTables(CommandList.GraphicsCommandList());
		return true;
	}

	void FD3D12StateCache::ClearState()
	{
		// Blend State Cache
		CurrentBlendFactor[0] = D3D12_DEFAULT_BLEND_FACTOR_RED;
		CurrentBlendFactor[1] = D3D12_DEFAULT_BLEND_FACTOR_GREEN;
		CurrentBlendFactor[2] = D3D12_DEFAULT_BLEND_FACTOR_BLUE;
		CurrentBlendFactor[3] = D3D12_DEFAULT_BLEND_FACTOR_ALPHA;

		CurrentReferenceStencil = 0;
		CurrentPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;

		PSDesc = {};
		CSDesc = {};
		ConstantBufferCache.Clear();
		SamplerCache.Clear();
		ShaderResourceViewCache.Clear();
		UAVCache.Clear();

		for (int32_t index = 0; index < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++index)
			CurrentDescriptorHeaps[index] = {};
	}

	void FD3D12StateCache::ClearRenderState()
	{
		for (int32_t index = 0; index < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++index)
			CurrentDescriptorHeaps[index] = {};
	}

	void FD3D12StateCache::ClearComputeState()
	{
		for (int32_t index = 0; index < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++index)
			CurrentDescriptorHeaps[index] = {};
	}

	void FD3D12StateCache::CleanupUsedHeaps(uint64_t FenceValue, ED3D12CommandQueueType QueueType)
	{
		DynamicViewDescriptorHeap.CleanupUsedHeaps(FenceValue, QueueType);
	}

}