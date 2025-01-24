#include "D3D12/D3D12StateCache.h"
#include "D3D12/D3D12Shaders.h"
#include "common/crc.h"
#include "D3D12/D3D12RootSignature.h"
#include "D3D12/D3D12CommandList.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12UniformBuffer.h"
#include "D3D12/D3D12ReourceTraits.h"
#include "D3D12/D3D12RenderTarget.h"

namespace RenderCore
{
	FD3D12StateCache::FD3D12StateCache(std::weak_ptr<FD3D12Device> InParent)
		:FD3D12DeviceChild(InParent)
		,DynamicViewDescriptorHeap(InParent,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
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

	void FD3D12StateCache::SetDescriptorHeap(D3D12CommandListHandle& CommandList, D3D12_DESCRIPTOR_HEAP_TYPE Type, win32::com_ptr<ID3D12DescriptorHeap> HeapPtr)
	{
		if (CurrentDescriptorHeaps[Type] != HeapPtr)
		{
			CurrentDescriptorHeaps[Type] = HeapPtr;
			BindDescriptorHeaps(CommandList);
		}
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
			D3D12Texture2D* Tex2D = RHIResourceCast(Targets[i].get());
			PSDesc.RTVFormats[i] = Tex2D->GetPlatformResourceFormat();
		}
		for (uint32_t i = (uint32_t)Targets.size(); i < MaxSimultaneousRenderTargets; ++i)
			PSDesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
		PSDesc.NumRenderTargets = (uint32_t)Targets.size();
		D3D12Texture2D* DepthTex = RHIResourceCast(Depth.get());
		PSDesc.DSVFormat = DepthTex ? DepthTex->GetPlatformResourceFormat() : DXGI_FORMAT_UNKNOWN;
		PSDesc.SampleDesc.Count = 1;
		PSDesc.SampleDesc.Quality = 0;
	}

	void FD3D12StateCache::SetRenderTargetFormat(const D3D12RenderTarget* RenderTarget)
	{
		D3D12Texture2D* Tex2D = RHIResourceCast(RenderTarget->GetTex().get());
		PSDesc.RTVFormats[0] = Tex2D->GetPlatformResourceFormat();
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
		
		CurrentRootHash = core::Crc::MemCrc32(KeyName.data(), (int32_t)KeyName.size());

		std::shared_ptr<FRootSignature> RootSignature;
		auto ItRootSignature = RootSignatures.find(CurrentRootHash);
		if (ItRootSignature != RootSignatures.end())
			return ItRootSignature->second;
		RootSignature = std::make_shared<FRootSignature>(GetParentDevice());

		int32_t NumRootParams = 0;
		NumRootParams += VertexResCount.NumCBs;
		NumRootParams += PixelResCount.NumCBs;
		if (PixelResCount.NumSRVs > 0)
			NumRootParams += 1;
		if (PixelResCount.NumUAVs > 0)
			NumRootParams += 1;

		RootSignature->Reset(NumRootParams, VertexResCount.NumSamplers + PixelResCount.NumSamplers);

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

		if (PixelResCount.NumSRVs > 0)
		{
			(*RootSignature)[RootIndex].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, PixelResCount.NumSRVs);
			RootSignature->SRVRootIndex[SF_Pixel] = RootIndex;
			++RootIndex;
		}

		if (PixelResCount.NumUAVs > 0)
		{
			(*RootSignature)[RootIndex].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, PixelResCount.NumUAVs);

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
		if (RootSignature->Finalize(core::ansi_ucs2(KeyName), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT))
		{
			RootSignatures.insert({ CurrentRootHash,RootSignature });
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

		FShaderCodePackedResourceCounts VertexResCount{};
		VertexResCount = itVertexShader->second->ResourceCounts;
		FShaderCodePackedResourceCounts PixelResCount{};
		PixelResCount = itPixelShader->second->ResourceCounts;

		CommandList.FlushResourceBarriers();

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
		m_InputLayouts.resize(ElementDescs.size());

		int32_t Index = 0;
		for (const auto& Item : ElementDescs)
		{
			D3D12_INPUT_ELEMENT_DESC& ElementDesc = m_InputLayouts[Index++];
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
			PSDesc.InputLayout.pInputElementDescs = m_InputLayouts.data();
		else
			PSDesc.InputLayout.pInputElementDescs = nullptr;

		size_t HashCode = core::Crc::HashState(&PSDesc);
		HashCode = core::Crc::HashState(m_InputLayouts.data(), PSDesc.InputLayout.NumElements, HashCode);
		
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
		ConstantBufferCache.Clear();
		SamplerCache.Clear();
		ShaderResourceViewCache.Clear();

		for (int32_t index = 0; index < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++index)
			CurrentDescriptorHeaps[index] = {};
	}

	void FD3D12StateCache::ClearRenderState()
	{
		ConstantBufferCache.Clear();
		SamplerCache.Clear();
		ShaderResourceViewCache.Clear();

		for (int32_t index = 0; index < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++index)
			CurrentDescriptorHeaps[index] = {};
	}

	void FD3D12StateCache::CleanupUsedHeaps(uint64_t FenceValue)
	{
		DynamicViewDescriptorHeap.CleanupUsedHeaps(FenceValue);
	}

}