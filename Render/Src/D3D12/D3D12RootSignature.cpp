#include "D3D12/D3D12RootSignature.h"
#include "D3D12/D3D12RootSignatureDefinitions.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12WindowDevice.h"
#include "core/logger.h"

namespace RenderCore
{

	void FRootSignature::InitStaticSampler(uint32_t Register, const D3D12_STATIC_SAMPLER_DESC& SamplerDesc, D3D12_SHADER_VISIBILITY Visibility /*= D3D12_SHADER_VISIBILITY_ALL*/)
	{
		Assert(m_StaticSamplerArray.size() < m_NumStaticSamplers);

		D3D12_STATIC_SAMPLER_DESC StaticSamplerDesc{};
		StaticSamplerDesc.Filter = SamplerDesc.Filter;
		StaticSamplerDesc.AddressU = SamplerDesc.AddressU;
		StaticSamplerDesc.AddressV = SamplerDesc.AddressV;
		StaticSamplerDesc.AddressW = SamplerDesc.AddressW;
		StaticSamplerDesc.MipLODBias = SamplerDesc.MipLODBias;
		StaticSamplerDesc.MaxAnisotropy = SamplerDesc.MaxAnisotropy;
		StaticSamplerDesc.ComparisonFunc = SamplerDesc.ComparisonFunc;
		StaticSamplerDesc.BorderColor = SamplerDesc.BorderColor;
		StaticSamplerDesc.MinLOD = SamplerDesc.MinLOD;
		StaticSamplerDesc.MaxLOD = SamplerDesc.MaxLOD;
		StaticSamplerDesc.ShaderRegister = Register;
		StaticSamplerDesc.RegisterSpace = 0;
		StaticSamplerDesc.ShaderVisibility = Visibility;

		m_StaticSamplerArray.emplace_back(StaticSamplerDesc);
	}

	bool FRootSignature::Finalize(const std::wstring& name, D3D12_ROOT_SIGNATURE_FLAGS Flags /*= D3D12_ROOT_SIGNATURE_FLAG_NONE*/)
	{
		if (m_Finalized)
			return true;

		Assert(m_StaticSamplerArray.size() == m_NumStaticSamplers);

		D3D12_VERSIONED_ROOT_SIGNATURE_DESC RootVersionedDesc{};
		RootVersionedDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1;
		D3D12_ROOT_SIGNATURE_DESC& RootDesc = RootVersionedDesc.Desc_1_0;
		RootDesc.Flags = Flags;
		RootDesc.NumParameters = m_NumParameters;
		if (m_NumParameters == 0)
			RootDesc.pParameters = nullptr;
		else
			RootDesc.pParameters = (const D3D12_ROOT_PARAMETER*)&m_ParamArray[0];
		RootDesc.NumStaticSamplers = m_NumStaticSamplers;
		if (m_NumStaticSamplers == 0)
			RootDesc.pStaticSamplers = nullptr;
		else
			RootDesc.pStaticSamplers = (const D3D12_STATIC_SAMPLER_DESC*)&m_StaticSamplerArray[0];

		m_DescriptorTableBitMap = 0;
		m_SamplerTableBitMap = 0;
		for (UINT i = 0; i < m_NumParameters; ++i)
		{
			const D3D12_ROOT_PARAMETER& RootParam = RootDesc.pParameters[i];
			m_DescriptorTableSize[i] = 0;

			if (RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
			{
				Assert(RootParam.DescriptorTable.pDescriptorRanges != nullptr);

				if (RootParam.DescriptorTable.pDescriptorRanges->RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
					m_SamplerTableBitMap |= (1 << i);
				else
					m_DescriptorTableBitMap |= (1 << i);

				for (UINT TableRange = 0; TableRange < RootParam.DescriptorTable.NumDescriptorRanges; ++TableRange)
				{
					m_DescriptorTableSize[i] += RootParam.DescriptorTable.pDescriptorRanges[TableRange].NumDescriptors;
				}
			}
		}

		win32::com_ptr<ID3DBlob> pOutBlob, pErrorBlob;
		HRESULT h = D3D12SerializeVersionedRootSignature(&RootVersionedDesc,pOutBlob.get_init_ref(), pErrorBlob.get_init_ref());
		VERIFYD3DRESULT(h);
		if (FAILED(h))
			return false;

		h = GetParentDevice()->GetDevice()->CreateRootSignature(1, pOutBlob->GetBufferPointer(), pOutBlob->GetBufferSize(), IID_PPV_ARGS(&m_D3DRootSignature));
		VERIFYD3DRESULT(h);
		if (FAILED(h))
			return false;

		m_D3DRootSignature->SetName(name.c_str());
		m_Finalized = true;
		return true;
	}

}