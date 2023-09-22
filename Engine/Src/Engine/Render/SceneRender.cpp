#include "Render/SceneRender.h"
#include "Scene/SceneView.h"
#include "RHI/RHICommandContext.h"
#include "Scene/Actor.h"
#include "Scene/Component.h"
#include "RHI/RHIViewPort.h"
#include "Thread/RenderThread.h"
#include "Engine.h"
#include "RHI/DynamicRHI.h"
#include "App/AppWindow.h"
#include "Render/PreProcessor.h"

using namespace RenderCore;

namespace Engine
{
	struct SceneRenderP
	{
		std::weak_ptr<SceneView> Owner;
		std::shared_ptr<RHIViewPort> MainViewPort;
		std::shared_ptr<PreProcessor> PreProcess;
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
		ENQUEUE_UNIQUE_RENDER_COMMAND(([Impl = Impl](RenderCore::DynamicRHI* RHI){
			if (!Impl->PreProcess)
			{
				Impl->PreProcess = std::make_shared<PreProcessor>(RHI);
			}
			Impl->PreProcess->InitResource();
		}));
	}

	void SceneRender::LoadConfig(const std::wstring& FileName)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND(([Impl = Impl, FileName](RenderCore::DynamicRHI* RHI) {
			if (Impl->PreProcess)
			{
				Impl->PreProcess->LoadConfig(FileName);
			}
		}));
	}

	void SceneRender::Resize(uint32_t InSizeX, uint32_t InSizeY, bool bInIsFullscreen)
	{
		if (InSizeX ==0 || InSizeY == 0)
		{
			return;
		}

		ENQUEUE_UNIQUE_RENDER_COMMAND(([Impl = Impl, InSizeX,InSizeY,bInIsFullscreen](RenderCore::DynamicRHI* RHI) {
			Impl->MainViewPort->Resize(InSizeX,InSizeY,bInIsFullscreen);
		}));
	}

	void SceneRender::Render()
	{
		std::shared_ptr<RHICommandContext> CommandContext =  GEngine->GetRHI()->GetDefaultCommandContext();
		if (!CommandContext)
		{
			return;
		}

		ENQUEUE_UNIQUE_RENDER_COMMAND(([Impl = Impl, CommandContext](RenderCore::DynamicRHI* RHI) {
			if (Impl->PreProcess)
			{
				Impl->PreProcess->Draw(*CommandContext);
			}
			}));

		auto ClearAndSetViewPort = [Impl = Impl](RenderCore::DynamicRHI* RHI) {
			Impl->MainViewPort->Clear(core::FLinearColor::Gray);
			Impl->MainViewPort->SetRenderTarget();
			int32_t width = GEngine->GetAppWindow()->GetWidth();
			int32_t height = GEngine->GetAppWindow()->GetHeight();
			RHI->GetDefaultCommandContext()->SetViewPort(0, 0, width, height);
		};

		ENQUEUE_UNIQUE_RENDER_COMMAND(ClearAndSetViewPort);


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

		auto Present = [Impl = Impl, this](RenderCore::DynamicRHI*) {
			Impl->MainViewPort->Present();
		};

		ENQUEUE_UNIQUE_RENDER_COMMAND(Present);

	}

}


