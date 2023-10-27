#pragma once
#include "core/inc.h"

namespace Engine
{
	struct GBufferPrivate;

	enum GBufferFlagBits
	{
		GBUFFER_NONE = 0,
		GBUFFER_DEPTH = 1,
		GBUFFER_MOTION_VECTORS = 2,
		GBUFFER_SCENE_COLOR = 8
	};

	class GBuffer
	{
	public:
		GBuffer();
		~GBuffer();

		void InitBuffer(GBufferFlagBits Flag);

	private:
		GBufferPrivate* d_ptr = nullptr;
	};
}