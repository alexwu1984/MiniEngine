#include "D3D12/D3D12StateCache.h"
#include "D3D12/D3D12Shaders.h"
#include "common/crc.h"
#include "D3D12/D3D12RootSignature.h"
#include "D3D12/D3D12CommandList.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12UniformBuffer.h"

namespace RenderCore
{

	FD3D12StateCache::FD3D12StateCache(std::weak_ptr<FD3D12Device> InParent)
		:FD3D12DeviceChild(InParent)
	{

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
			bNeedSetPrimitiveTopology = true;
		}
	}

	void FD3D12StateCache::SetDynamicConstantBuffer(uint32_t RootIndex, std::shared_ptr<D3D12UniformBuffer> UniformBuffer)
	{
		DynamicConstantBuffers[RootIndex] = UniformBuffer;
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
			KeyName += itFindPixelShader->second->KeyName;
		}
		
		CurrentRootHash = core::Crc::MemCrc32(KeyName.data(), KeyName.size());

		std::shared_ptr<FRootSignature> RootSignature;
		auto ItRootSignature = RootSignatures.find(CurrentRootHash);
		if (ItRootSignature != RootSignatures.end())
			return ItRootSignature->second;
		RootSignature = std::make_shared<FRootSignature>(GetParentDevice());

		int32_t NumRootParams = 0;
		if (VertexResCount.NumCBs > 0)
			NumRootParams += 1;

		if (PixelResCount.NumCBs > 0)
			NumRootParams += 1;
		if (PixelResCount.NumSRVs > 0)
			NumRootParams += 1;
		if (PixelResCount.NumUAVs > 0)
			NumRootParams += 1;

		RootSignature->Reset(NumRootParams, VertexResCount.NumSamplers + PixelResCount.NumSamplers);

		int32_t RootIndex = 0;
		if (VertexResCount.NumCBs > 0)
			(*RootSignature)[RootIndex++].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_VERTEX);

		if (PixelResCount.NumCBs > 0)
			(*RootSignature)[RootIndex++].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_PIXEL);

		if (PixelResCount.NumSRVs > 0 )
			(*RootSignature)[RootIndex++].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, PixelResCount.NumSRVs);

		if (PixelResCount.NumUAVs > 0)
			(*RootSignature)[RootIndex++].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, PixelResCount.NumUAVs);

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
		if (RootSignature->Finalize(core::ansi_ucs2(KeyName), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT))\
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

		PSDesc.pRootSignature = RootSignature->GetSignature();
		Assert(PSDesc.pRootSignature != nullptr);

		auto itVertexShader = VertexShaders.find(CurrentVertexHash);
		if (itVertexShader == VertexShaders.end())
		{
			Assert(false);
			return false;
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
		PSDesc.InputLayout.NumElements = ElementDescs.size();
		PSDesc.InputLayout.pInputElementDescs = m_InputLayouts.data();

		size_t HashCode = core::Crc::HashState(&PSDesc);
		HashCode = core::Crc::HashState(m_InputLayouts.data(), PSDesc.InputLayout.NumElements, HashCode);
		
		{
			auto iter = GraphicsPSHashMap.find(HashCode);
			if (iter == GraphicsPSHashMap.end())
			{
				HRESULT hr = GetParentDevice()->GetDevice()->CreateGraphicsPipelineState(&PSDesc, IID_PPV_ARGS(&PipelineState));
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
		CommandList->SetPipelineState(PipelineState.get());

		for (auto it = DynamicConstantBuffers.begin(); it != DynamicConstantBuffers.end(); ++it)
		{
			CommandList->SetGraphicsRootConstantBufferView(it->first, it->second->GetGPUVirtualAddress());
		}

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
		DynamicConstantBuffers.clear();
	}

}