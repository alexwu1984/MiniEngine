#pragma once
#include "RHI/RHIShdader.h"

namespace RenderCore
{
	/** Use AttributeIndex as D3D input slot (one VB stream per ATTRIBUTE — GLTF mesh default). */
	static constexpr uint32_t kVertexDeclareSlotSameAsAttribute = UINT32_MAX;

	/**
	 * One vertex shader input element. Maps to HLSL `: ATTRIBUTE{n}` and D3D INPUT_ELEMENT_DESC.
	 *
	 * - AttributeIndex : semantic index (ATTRIBUTE0 = 0, ATTRIBUTE1 = 1, ...)
	 * - ElementType    : float1/2/3/4 component count in the VB
	 * - InputSlot      : which IA vertex buffer slot to read from (default: same as AttributeIndex)
	 * - ByteOffsetInSlot : byte offset inside that VB (0 for separable streams; non-zero for interleaved)
	 */
	struct FVertexDeclareElementSpec
	{
		uint8_t AttributeIndex = 0;
		EVertexElementType ElementType = EVertexElementType::VET_None;
		uint32_t InputSlot = kVertexDeclareSlotSameAsAttribute;
		uint32_t ByteOffsetInSlot = 0;
		bool bUseInstanceIndex = false;
	};

	/** Build RHIVertexDeclare from an explicit table (preferred over string parsing). */
	void BuildVertexDeclareFromSpec(const FVertexDeclareElementSpec* Elements, size_t ElementCount,
									RHIVertexDeclare& OutDeclare, bool bAppend = false);

	inline void BuildVertexDeclareFromSpec(std::initializer_list<FVertexDeclareElementSpec> Elements,
										   RHIVertexDeclare& OutDeclare, bool bAppend = false)
	{
		BuildVertexDeclareFromSpec(Elements.begin(), Elements.size(), OutDeclare, bAppend);
	}
}
