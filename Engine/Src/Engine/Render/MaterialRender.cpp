#include "Engine/Render/MaterialRender.h"
#include "Engine/Render/MaterialRenderP.h"
#include "Engine/Thread/RenderThread.h"

namespace Engine
{

	MaterialRender::~MaterialRender()
	{
		if (GRenderThread)
		{
			GRenderThread->WaitForFinish();
		}
	}

}