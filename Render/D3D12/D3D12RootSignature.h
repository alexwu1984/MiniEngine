#pragma once
#include "RHIPrivate/D3D12RHIPrivate.h"
#include "D3D12/D3D12RHICommon.h"

namespace RenderCore
{
	enum ERootParameterKeys
	{
		PS_SRVs,
		PS_CBVs,
		PS_RootCBVs,
		PS_Samplers,
		VS_SRVs,
		VS_CBVs,
		VS_RootCBVs,
		VS_Samplers,
		GS_SRVs,
		GS_CBVs,
		GS_RootCBVs,
		GS_Samplers,
		HS_SRVs,
		HS_CBVs,
		HS_RootCBVs,
		HS_Samplers,
		DS_SRVs,
		DS_CBVs,
		DS_RootCBVs,
		DS_Samplers,
		ALL_SRVs,
		ALL_CBVs,
		ALL_RootCBVs,
		ALL_Samplers,
		ALL_UAVs,
		RPK_RootParameterKeyCount,
	};

	class FD3D12RootSignatureDesc
	{
	public:
		explicit FD3D12RootSignatureDesc(const FD3D12QuantizedBoundShaderState& QBSS, const D3D12_RESOURCE_BINDING_TIER ResourceBindingTier);

		inline const D3D12_VERSIONED_ROOT_SIGNATURE_DESC& GetDesc() const { return RootDesc; }

		static D3D12_VERSIONED_ROOT_SIGNATURE_DESC& GetStaticGraphicsRootSignatureDesc();
		static D3D12_VERSIONED_ROOT_SIGNATURE_DESC& GetStaticComputeRootSignatureDesc();

		static constexpr uint32_t MaxRootParameters = 32;	// Arbitrary max, increase as needed.

	private:

		uint32_t RootParametersSize;	// The size of all root parameters in the root signature. Size in DWORDs, the limit is 64.
		CD3DX12_ROOT_PARAMETER1 TableSlots[MaxRootParameters];
		CD3DX12_DESCRIPTOR_RANGE1 DescriptorRanges[MaxRootParameters];
		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC RootDesc;
	};

	class FD3D12RootSignature : public FD3D12AdapterChild
	{
	private:
		// Struct for all the useful info we want per shader stage.
		struct ShaderStage
		{
			ShaderStage()
				: MaxCBVCount(0u)
				, MaxSRVCount(0u)
				, MaxSamplerCount(0u)
				, MaxUAVCount(0u)
				, CBVRegisterMask(0u)
				, bVisible(false)
			{
			}

			// TODO: Make these arrays and index into them by type instead of individual variables.
			uint8_t MaxCBVCount;
			uint8_t MaxSRVCount;
			uint8_t MaxSamplerCount;
			uint8_t MaxUAVCount;
			CBVSlotMask CBVRegisterMask;
			bool bVisible;
		};

	public:
		explicit FD3D12RootSignature(std::weak_ptr<FD3D12Adapter> InParent)
			: FD3D12AdapterChild(InParent)
		{}
		explicit FD3D12RootSignature(std::weak_ptr<FD3D12Adapter> InParent, const FD3D12QuantizedBoundShaderState& InQBSS)
			: FD3D12AdapterChild(InParent)
		{
			Init(InQBSS);
		}
		explicit FD3D12RootSignature(std::weak_ptr<FD3D12Adapter> InParent, const D3D12_VERSIONED_ROOT_SIGNATURE_DESC& InDesc, uint32_t BindingSpace = 0)
			: FD3D12AdapterChild(InParent)
		{
			Init(InDesc, BindingSpace);
		}
		explicit FD3D12RootSignature(std::weak_ptr<FD3D12Adapter> InParent, ID3DBlob* const InBlob, uint32_t BindingSpace = 0)
			: FD3D12AdapterChild(InParent)
		{
			Init(InBlob, BindingSpace);
		}

		void Init(const FD3D12QuantizedBoundShaderState& InQBSS);
		void Init(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC& InDesc, uint32_t BindingSpace = 0);
		void Init(ID3DBlob* const InBlob, uint32_t BindingSpace = 0);

		ID3D12RootSignature* GetRootSignature() const { return RootSignature.get(); }
		ID3DBlob* GetRootSignatureBlob() const { return RootSignatureBlob.get(); }

		inline uint32_t SamplerRDTBindSlot(EShaderFrequency ShaderStage) const
		{
			switch (ShaderStage)
			{
			case SF_Vertex: return BindSlotMap[VS_Samplers];
			case SF_Pixel: return BindSlotMap[PS_Samplers];
			case SF_Geometry: return BindSlotMap[GS_Samplers];
			case SF_Hull: return BindSlotMap[HS_Samplers];
			case SF_Domain: return BindSlotMap[DS_Samplers];
			case SF_Compute: return BindSlotMap[ALL_Samplers];

			default: assert(false);
				return UINT_MAX;
			}
		}

		inline uint32_t SRVRDTBindSlot(EShaderFrequency ShaderStage) const
		{
			switch (ShaderStage)
			{
			case SF_Vertex: return BindSlotMap[VS_SRVs];
			case SF_Pixel: return BindSlotMap[PS_SRVs];
			case SF_Geometry: return BindSlotMap[GS_SRVs];
			case SF_Hull: return BindSlotMap[HS_SRVs];
			case SF_Domain: return BindSlotMap[DS_SRVs];
			case SF_Compute: return BindSlotMap[ALL_SRVs];

			default: assert(false);
				return UINT_MAX;
			}
		}

		inline uint32_t CBVRDTBindSlot(EShaderFrequency ShaderStage) const
		{
			switch (ShaderStage)
			{
			case SF_Vertex: return BindSlotMap[VS_CBVs];
			case SF_Pixel: return BindSlotMap[PS_CBVs];
			case SF_Geometry: return BindSlotMap[GS_CBVs];
			case SF_Hull: return BindSlotMap[HS_CBVs];
			case SF_Domain: return BindSlotMap[DS_CBVs];
			case SF_Compute: return BindSlotMap[ALL_CBVs];

			default: assert(false);
				return UINT_MAX;
			}
		}

		inline uint32_t CBVRDBaseBindSlot(EShaderFrequency ShaderStage) const
		{
			switch (ShaderStage)
			{
			case SF_Vertex: return BindSlotMap[VS_RootCBVs];
			case SF_Pixel: return BindSlotMap[PS_RootCBVs];
			case SF_Geometry: return BindSlotMap[GS_RootCBVs];
			case SF_Hull: return BindSlotMap[HS_RootCBVs];
			case SF_Domain: return BindSlotMap[DS_RootCBVs];

			case SF_NumFrequencies:
			case SF_Compute: return BindSlotMap[ALL_RootCBVs];

			default: assert(false);
				return UINT_MAX;
			}
		}

		inline uint32_t CBVRDBindSlot(EShaderFrequency ShaderStage, uint32_t BufferIndex) const
		{
			// This code assumes that all Root CBVs for a particular stage are contiguous in the root signature (thus indexable by the buffer index).
			return CBVRDBaseBindSlot(ShaderStage) + BufferIndex;
		}

		inline uint32_t UAVRDTBindSlot(EShaderFrequency ShaderStage) const
		{
			assert(ShaderStage == SF_Pixel || ShaderStage == SF_Compute);
			return BindSlotMap[ALL_UAVs];
		}

		inline bool HasUAVs() const { return bHasUAVs; }
		inline bool HasSRVs() const { return bHasSRVs; }
		inline bool HasCBVs() const { return bHasCBVs; }
		inline bool HasSamplers() const { return bHasSamplers; }
		inline bool HasVS() const { return Stage[SF_Vertex].bVisible; }
		inline bool HasHS() const { return Stage[SF_Hull].bVisible; }
		inline bool HasDS() const { return Stage[SF_Domain].bVisible; }
		inline bool HasGS() const { return Stage[SF_Geometry].bVisible; }
		inline bool HasPS() const { return Stage[SF_Pixel].bVisible; }
		inline bool HasCS() const { return Stage[SF_Compute].bVisible; }	// Root signatures can be used for Graphics and/or Compute because they exist in separate bind spaces.
		inline uint32_t MaxSamplerCount(uint32_t ShaderStage) const { assert(ShaderStage != SF_NumFrequencies); return Stage[ShaderStage].MaxSamplerCount; }
		inline uint32_t MaxSRVCount(uint32_t ShaderStage) const { assert(ShaderStage != SF_NumFrequencies); return Stage[ShaderStage].MaxSRVCount; }
		inline uint32_t MaxCBVCount(uint32_t ShaderStage) const { assert(ShaderStage != SF_NumFrequencies); return Stage[ShaderStage].MaxCBVCount; }
		inline uint32_t MaxUAVCount(uint32_t ShaderStage) const { assert(ShaderStage != SF_NumFrequencies); return Stage[ShaderStage].MaxUAVCount; }
		inline CBVSlotMask CBVRegisterMask(uint32_t ShaderStage) const { assert(ShaderStage != SF_NumFrequencies); return Stage[ShaderStage].CBVRegisterMask; }

		uint32_t GetBindSlotOffsetInBytes(uint8_t BindSlotIndex) const { assert(BindSlotIndex < _ARRAYSIZE(BindSlotOffsetsInDWORDs)); return 4 * BindSlotOffsetsInDWORDs[BindSlotIndex]; }
		uint32_t GetTotalRootSignatureSizeInBytes() const { return 4 * TotalRootSignatureSizeInDWORDs; }

	private:
		void AnalyzeSignature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC& Desc, uint32_t BindingSpace);

		template<typename RootSignatureDescType>
		void InternalAnalyzeSignature(const RootSignatureDescType& Desc, uint32_t BindingSpace);

		inline bool HasVisibility(const D3D12_SHADER_VISIBILITY& ParameterVisibility, const D3D12_SHADER_VISIBILITY& Visibility) const
		{
			return ParameterVisibility == D3D12_SHADER_VISIBILITY_ALL || ParameterVisibility == Visibility;
		}

		inline void SetSamplersRDTBindSlot(EShaderFrequency SF, uint8_t RootParameterIndex)
		{
			uint8_t* pBindSlot = nullptr;
			switch (SF)
			{
			case SF_Vertex: pBindSlot = &BindSlotMap[VS_Samplers]; break;
			case SF_Pixel: pBindSlot = &BindSlotMap[PS_Samplers]; break;
			case SF_Geometry: pBindSlot = &BindSlotMap[GS_Samplers]; break;
			case SF_Hull: pBindSlot = &BindSlotMap[HS_Samplers]; break;
			case SF_Domain: pBindSlot = &BindSlotMap[DS_Samplers]; break;

			case SF_Compute:
			case SF_NumFrequencies: pBindSlot = &BindSlotMap[ALL_Samplers]; break;

			default: assert(false);
				return;
			}

			assert(*pBindSlot == 0xFF);
			*pBindSlot = RootParameterIndex;

			bHasSamplers = true;
		}

		inline void SetSRVRDTBindSlot(EShaderFrequency SF, uint8_t RootParameterIndex)
		{
			uint8_t* pBindSlot = nullptr;
			switch (SF)
			{
			case SF_Vertex: pBindSlot = &BindSlotMap[VS_SRVs]; break;
			case SF_Pixel: pBindSlot = &BindSlotMap[PS_SRVs]; break;
			case SF_Geometry: pBindSlot = &BindSlotMap[GS_SRVs]; break;
			case SF_Hull: pBindSlot = &BindSlotMap[HS_SRVs]; break;
			case SF_Domain: pBindSlot = &BindSlotMap[DS_SRVs]; break;

			case SF_Compute:
			case SF_NumFrequencies: pBindSlot = &BindSlotMap[ALL_SRVs]; break;

			default: assert(false);
				return;
			}

			assert(*pBindSlot == 0xFF);
			*pBindSlot = RootParameterIndex;

			bHasSRVs = true;
		}

		inline void SetCBVRDTBindSlot(EShaderFrequency SF, uint8_t RootParameterIndex)
		{
			uint8_t* pBindSlot = nullptr;
			switch (SF)
			{
			case SF_Vertex: pBindSlot = &BindSlotMap[VS_CBVs]; break;
			case SF_Pixel: pBindSlot = &BindSlotMap[PS_CBVs]; break;
			case SF_Geometry: pBindSlot = &BindSlotMap[GS_CBVs]; break;
			case SF_Hull: pBindSlot = &BindSlotMap[HS_CBVs]; break;
			case SF_Domain: pBindSlot = &BindSlotMap[DS_CBVs]; break;

			case SF_Compute:
			case SF_NumFrequencies: pBindSlot = &BindSlotMap[ALL_CBVs]; break;

			default: assert(false);
				return;
			}

			assert(*pBindSlot == 0xFF);
			*pBindSlot = RootParameterIndex;

			bHasCBVs = true;
			bHasRDTCBVs = true;
		}

		inline void SetCBVRDBindSlot(EShaderFrequency SF, uint8_t RootParameterIndex)
		{
			uint8_t* pBindSlot = nullptr;
			switch (SF)
			{
			case SF_Vertex: pBindSlot = &BindSlotMap[VS_RootCBVs]; break;
			case SF_Pixel: pBindSlot = &BindSlotMap[PS_RootCBVs]; break;
			case SF_Geometry: pBindSlot = &BindSlotMap[GS_RootCBVs]; break;
			case SF_Hull: pBindSlot = &BindSlotMap[HS_RootCBVs]; break;
			case SF_Domain: pBindSlot = &BindSlotMap[DS_RootCBVs]; break;

			case SF_Compute:
			case SF_NumFrequencies: pBindSlot = &BindSlotMap[ALL_RootCBVs]; break;

			default: assert(false);
				return;
			}

			assert(*pBindSlot == 0xFF);
			*pBindSlot = RootParameterIndex;

			bHasCBVs = true;
			bHasRDCBVs = true;
		}

		inline void SetUAVRDTBindSlot(EShaderFrequency SF, uint8_t RootParameterIndex)
		{
			assert(SF == SF_Pixel || SF == SF_Compute || SF == SF_NumFrequencies);
			uint8_t* pBindSlot = &BindSlotMap[ALL_UAVs];

			assert(*pBindSlot == 0xFF);
			*pBindSlot = RootParameterIndex;

			bHasUAVs = true;
		}

		inline void SetMaxSamplerCount(EShaderFrequency SF, uint8_t Count)
		{
			if (SF == SF_NumFrequencies)
			{
				// Update all counts for all stages.
				for (uint32_t s = SF_Vertex; s <= SF_Compute; s++)
				{
					Stage[s].MaxSamplerCount = Count;
				}
			}
			else
			{
				Stage[SF].MaxSamplerCount = Count;
			}
		}

		inline void SetMaxSRVCount(EShaderFrequency SF, uint8_t Count)
		{
			if (SF == SF_NumFrequencies)
			{
				// Update all counts for all stages.
				for (uint32_t s = SF_Vertex; s <= SF_Compute; s++)
				{
					Stage[s].MaxSRVCount = Count;
				}
			}
			else
			{
				Stage[SF].MaxSRVCount = Count;
			}
		}

		// Update the mask that indicates what shader registers are used in the descriptor table.
		template<typename DescriptorRangeType>
		inline void UpdateCBVRegisterMaskWithDescriptorRange(EShaderFrequency SF, const DescriptorRangeType& Range)
		{
			const uint32_t StartRegister = Range.BaseShaderRegister;
			const uint32_t EndRegister = StartRegister + Range.NumDescriptors;
			const uint32_t StartStage = (SF == SF_NumFrequencies) ? SF_Vertex : SF;
			const uint32_t EndStage = (SF == SF_NumFrequencies) ? SF_Compute : SF;
			for (uint32_t CurrentStage = StartStage; CurrentStage <= EndStage; CurrentStage++)
			{
				for (uint32_t Register = StartRegister; Register < EndRegister; Register++)
				{
					// The bit shouldn't already be set for the current register.
					assert((Stage[CurrentStage].CBVRegisterMask & (1 << Register)) == 0);
					Stage[CurrentStage].CBVRegisterMask |= (1 << Register);
				}
			}
		}

		// Update the mask that indicates what shader registers are used in the root descriptor.
		template<typename DescriptorType>
		inline void UpdateCBVRegisterMaskWithDescriptor(EShaderFrequency SF, const DescriptorType& Descriptor)
		{
			const uint32_t StartStage = (SF == SF_NumFrequencies) ? SF_Vertex : SF;
			const uint32_t EndStage = (SF == SF_NumFrequencies) ? SF_Compute : SF;
			const uint32_t& Register = Descriptor.ShaderRegister;
			for (uint32_t CurrentStage = StartStage; CurrentStage <= EndStage; CurrentStage++)
			{
				// The bit shouldn't already be set for the current register.
				assert((Stage[CurrentStage].CBVRegisterMask & (1 << Register)) == 0);
				Stage[CurrentStage].CBVRegisterMask |= (1 << Register);
			}
		}

		inline void SetMaxCBVCount(EShaderFrequency SF, uint8_t Count)
		{
			if (SF == SF_NumFrequencies)
			{
				// Update all counts for all stages.
				for (uint32_t s = SF_Vertex; s <= SF_Compute; s++)
				{
					Stage[s].MaxCBVCount = Count;
				}
			}
			else
			{
				Stage[SF].MaxCBVCount = Count;
			}
		}

		inline void IncrementMaxCBVCount(EShaderFrequency SF, uint8_t Count)
		{
			if (SF == SF_NumFrequencies)
			{
				// Update all counts for all stages.
				for (uint32_t s = SF_Vertex; s <= SF_Compute; s++)
				{
					Stage[s].MaxCBVCount += Count;
				}
			}
			else
			{
				Stage[SF].MaxCBVCount += Count;
			}
		}

		inline void SetMaxUAVCount(EShaderFrequency SF, uint8_t Count)
		{
			if (SF == SF_NumFrequencies)
			{
				// Update all counts for all stages.
				for (uint32_t s = SF_Vertex; s <= SF_Compute; s++)
				{
					Stage[s].MaxUAVCount = Count;
				}
			}
			else
			{
				Stage[SF].MaxUAVCount = Count;
			}
		}
	private:
		win32::com_ptr<ID3D12RootSignature> RootSignature;
		uint8_t BindSlotMap[RPK_RootParameterKeyCount];	// This map uses an enum as a key to lookup the root parameter index
		ShaderStage Stage[SF_NumFrequencies];
		bool bHasUAVs;
		bool bHasSRVs;
		bool bHasCBVs;
		bool bHasRDTCBVs;
		bool bHasRDCBVs;
		bool bHasSamplers;
		win32::com_ptr<ID3DBlob> RootSignatureBlob;

		uint8_t BindSlotOffsetsInDWORDs[FD3D12RootSignatureDesc::MaxRootParameters] = {};
		uint8_t TotalRootSignatureSizeInDWORDs = 0;
	};

	class FRootParameter
	{
		friend class FRootSignature;

	public:
		FRootParameter()
		{
			m_RootParam.ParameterType = (D3D12_ROOT_PARAMETER_TYPE)-1;
		}
		~FRootParameter()
		{
			Clear();
		}

		void Clear()
		{
			if (m_RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
			{
				delete[] m_RootParam.DescriptorTable.pDescriptorRanges;
			}
			m_RootParam.ParameterType = (D3D12_ROOT_PARAMETER_TYPE)-1;
		}

		void InitAsConstants(UINT Register, UINT NumDwords, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL)
		{
			m_RootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
			m_RootParam.ShaderVisibility = Visibility;
			m_RootParam.Constants.Num32BitValues = NumDwords;
			m_RootParam.Constants.ShaderRegister = Register;
			m_RootParam.Constants.RegisterSpace = 0;
		}

		void InitAsBufferCBV(UINT Register, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL)
		{
			m_RootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			m_RootParam.ShaderVisibility = Visibility;
			m_RootParam.Descriptor.ShaderRegister = Register;
			m_RootParam.Descriptor.RegisterSpace = 0;
		}

		void InitAsBufferSRV(UINT Register, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL)
		{
			m_RootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
			m_RootParam.ShaderVisibility = Visibility;
			m_RootParam.Descriptor.ShaderRegister = Register;
			m_RootParam.Descriptor.RegisterSpace = 0;
		}

		void InitAsBufferUAV(UINT Register, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL)
		{
			m_RootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
			m_RootParam.ShaderVisibility = Visibility;
			m_RootParam.Descriptor.ShaderRegister = Register;
			m_RootParam.Descriptor.RegisterSpace = 0;
		}

		void InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE Type, UINT Register, UINT Count, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL)
		{
			InitAsDescriptorTable(1, Visibility);
			SetTableRange(0, Type, Register, Count);
		}

		void InitAsDescriptorTable(UINT RangeCount, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL)
		{
			m_RootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			m_RootParam.ShaderVisibility = Visibility;
			m_RootParam.DescriptorTable.NumDescriptorRanges = RangeCount;
			m_RootParam.DescriptorTable.pDescriptorRanges = new D3D12_DESCRIPTOR_RANGE[RangeCount];
		}

		void SetTableRange(UINT RangeIndex, D3D12_DESCRIPTOR_RANGE_TYPE Type, UINT Register, UINT Count, UINT Space = 0)
		{
			D3D12_DESCRIPTOR_RANGE* range = const_cast<D3D12_DESCRIPTOR_RANGE*>(m_RootParam.DescriptorTable.pDescriptorRanges + RangeIndex);
			range->RangeType = Type;
			range->NumDescriptors = Count;
			range->BaseShaderRegister = Register;
			range->RegisterSpace = Space;
			range->OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		}

	protected:
		D3D12_ROOT_PARAMETER m_RootParam;
	};

	class FRootSignature : public FD3D12DeviceChild
	{
	public:
		FRootSignature(std::weak_ptr<FD3D12Device> InParent ,uint32_t NumRootParams = 0, uint32_t NumStaticSamplers = 0)
			:FD3D12DeviceChild(InParent)
		{
			Reset(NumRootParams, NumStaticSamplers);
		}

		~FRootSignature() = default;

		void Reset(uint32_t NumRootParams, uint32_t NumStaticSamplers)
		{
			m_ParamArray.resize(NumRootParams);
			m_NumParameters = NumRootParams;

			m_StaticSamplerArray.clear();
			m_NumStaticSamplers = NumStaticSamplers;
		}

		FRootParameter& operator[] (size_t EntryIndex)
		{
			Assert(EntryIndex < m_NumParameters);
			return m_ParamArray[EntryIndex];
		}

		const FRootParameter& operator[] (size_t EntryIndex) const
		{
			Assert(EntryIndex < m_NumParameters);
			return m_ParamArray[EntryIndex];
		}

		void InitStaticSampler(uint32_t Register, const D3D12_STATIC_SAMPLER_DESC& SamplerDesc, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL);

		void Finalize(const std::wstring& name, D3D12_ROOT_SIGNATURE_FLAGS Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE);

		ID3D12RootSignature* GetSignature() const { return m_D3DRootSignature.get(); }

		uint32_t GetNumParameters() const { return m_NumParameters; }

		uint32_t GetSamplerTableBitMap() const { return m_SamplerTableBitMap; }
		uint32_t GetDescriptorTableBitMap() const { return m_DescriptorTableBitMap; }
		uint32_t GetDescriptorTableSize(uint32_t RootIndex) const { return m_DescriptorTableSize[RootIndex]; }


	protected:
		bool m_Finalized = false;
		uint32_t m_NumParameters = 0;
		uint32_t m_NumStaticSamplers = 0;

		uint32_t m_DescriptorTableBitMap = 0;
		uint32_t m_SamplerTableBitMap = 0;
		uint32_t m_DescriptorTableSize[16] = {};

		std::vector<FRootParameter> m_ParamArray;
		std::vector< D3D12_STATIC_SAMPLER_DESC> m_StaticSamplerArray;
		win32::com_ptr<ID3D12RootSignature> m_D3DRootSignature;
	};
}
