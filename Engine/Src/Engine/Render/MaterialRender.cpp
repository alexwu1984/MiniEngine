#include "Engine/Render/MaterialRender.h"
#include "Engine/Render/MaterialRenderP.h"
#include "Engine/Thread/RenderThread.h"

namespace Engine
{

	MaterialRender::~MaterialRender()
	{
		if (!GRenderThread)
			return;
		// FMeshMaterialRenderCache::Clear() runs on the render worker; never wait for that same thread here.
		if (std::this_thread::get_id() != GRenderThread->GetWorkerThreadId())
			GRenderThread->WaitForFinish();
	}

}