#include "D3D12/D3D12StateCache.h"
#include "D3D12/D3D12Shaders.h"
#include "common/crc.h"
#include "D3D12/D3D12RootSignature.h"

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

	bool FD3D12StateCache::BuildFootSignature()
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
		
		uint32_t Hash = core::Crc::MemCrc32(KeyName.data(), KeyName.size());

		std::shared_ptr<FRootSignature> RootSignature;
		auto ItRootSignature = RootSignatures.find(Hash);
		if (ItRootSignature != RootSignatures.end())
			return true;
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
			RootSignatures.insert({ Hash,RootSignature });
			return true;
		}
		else
			return false;
	}

}