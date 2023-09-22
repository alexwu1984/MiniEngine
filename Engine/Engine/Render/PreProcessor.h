#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
}

namespace Engine
{
	struct PreProcessorPrivate;
	class PreProcessor
	{
	public:
		PreProcessor(RenderCore::DynamicRHI* RHI);
		~PreProcessor();

		void InitResource();
		void LoadConfig(const std::wstring& FileName);
		void Draw(RenderCore::RHICommandContext& RHIContext);

	private:
		PreProcessorPrivate* d_ptr = nullptr;
	};
}
