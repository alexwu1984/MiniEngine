#include "RHI/VertexDeclareConfig.h"

namespace RenderCore
{
	void BuildVertexDeclareFromSpec(const FVertexDeclareElementSpec* Elements, size_t ElementCount,
									RHIVertexDeclare& OutDeclare, bool bAppend)
	{
		if (!Elements || ElementCount == 0)
			return;

		if (!bAppend)
			OutDeclare = RHIVertexDeclare{};

		for (size_t i = 0; i < ElementCount; ++i)
		{
			const FVertexDeclareElementSpec& Spec = Elements[i];
			const uint32_t inputSlot = (Spec.InputSlot == kVertexDeclareSlotSameAsAttribute)
				? static_cast<uint32_t>(Spec.AttributeIndex)
				: Spec.InputSlot;
			const VertexDeclareInput input(Spec.AttributeIndex, Spec.ElementType, Spec.bUseInstanceIndex);
			OutDeclare.AppendDeclareInput(input, inputSlot, Spec.ByteOffsetInSlot);
		}
	}
}
