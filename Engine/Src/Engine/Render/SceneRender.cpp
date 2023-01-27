#include "Render/SceneRender.h"
#include "Scene/SceneView.h"
#include "RHI/RHICommandContext.h"
#include "Scene/Actor.h"
#include "Scene/Component.h"
#include "RHI/RHIViewPort.h"
#include "Thread/RenderThread.h"
#include "Engine.h"
#include "RHI/DynamicRHI.h"

using namespace RenderCore;

namespace Engine
{
	struct SceneRenderP
	{
		std::weak_ptr<SceneView> Owner;
		std::shared_ptr<RHIViewPort> MainViewPort;

	};
	
	SceneRender::SceneRender(std::weak_ptr<SceneView> Owner)
		:Impl(std::make_shared<SceneRenderP>())
	{
		Impl->Owner = Owner;
	}

	SceneRender::~SceneRender()
	{
		if (GRenderThread)
		{
			GRenderThread->WaitForFinish();
		}
	}

	std::shared_ptr<SceneView> SceneRender::GetOwner() const
	{
		return Impl->Owner.lock();
	}

	void SceneRender::InitResource(std::shared_ptr<RHIViewPort> ViewPort)
	{
		Impl->MainViewPort = ViewPort;
	}

	void SceneRender::Render()
	{
		std::shared_ptr<RHICommandContext> CommandContext =  GEngine->GetRHI()->GetDefaultCommandContext();
		if (!CommandContext)
		{
			return;
		}

		ENQUEUE_UNIQUE_RENDER_COMMAND(([Impl = Impl](RenderCore::DynamicRHI* ) {
			Impl->MainViewPort->Clear(core::FLinearColor::Blue);

		}));

		const auto& Actors = GetOwner()->GetAllActors();
		for (const auto& ActorItem: Actors )
		{
			if (ActorItem->GetState() == Actor::EActive)
			{
				auto& Components = ActorItem->GetComponents();
				for (auto& ComponentItem : Components)
				{
					ComponentItem->Draw(*CommandContext, GetOwner()->GetMainCamera());
				}
			}
		}

		ENQUEUE_UNIQUE_RENDER_COMMAND(([Impl = Impl](RenderCore::DynamicRHI* ) {
			Impl->MainViewPort->Present();
		}));

		
	}

}


