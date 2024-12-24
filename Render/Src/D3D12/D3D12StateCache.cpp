#include "D3D12/D3D12StateCache.h"
#include "D3D12/D3D12Shaders.h"
#include "common/crc.h"
#include "D3D12/D3D12RootSignature.h"
#include "D3D12/D3D12CommandList.h"
#include "D3D12/D3D12WindowDevice.h"

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
		}
		else
		{
			CurrentVertexHash = 0;
		}
	}

	void FD3D12StateCache::SetPixelShader(std::shared_ptr<FD3D12PixelShader> InPixelShader)
	{
		if (InPixelShader)
		{
			if(PixelShaders.count(InPixelShader->Hash) == 0)
				PixelShaders.insert({ InPixelShader->Hash, InPixelShader });
			CurrentPixelHash = InPixelShader->Hash;
		}
		else
		{
			CurrentPixelHash = 0;
		}
	}

	void FD3D12StateCache::SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology)
	{
		if (CurrentPrimitiveTopology != PrimitiveTopology)
		{
			CurrentPrimitiveTopology = PrimitiveTopology;
			bNeedSetPrimitiveTopology = true;
		}
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

		PSDesc.InputLayout.pInputElementDescs = nullptr;
		size_t HashCode = core::Crc::HashState(&PSDesc);
		HashCode = core::Crc::HashState(m_InputLayouts, PSDesc.InputLayout.NumElements, HashCode);
		PSDesc.InputLayout.pInputElementDescs = m_InputLayouts;

		{
			auto iter = GraphicsPSHashMap.find(HashCode);
			if (iter == GraphicsPSHashMap.end())
			{
				VERIFYD3DRESULT(GetParentDevice()->GetDevice()->CreateGraphicsPipelineState(&PSDesc, IID_PPV_ARGS(&PipelineState)));
				GraphicsPSHashMap[HashCode] = PipelineState;
			}
			else
			{
				PipelineState = GraphicsPSHashMap[HashCode];
			}
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
	}

}