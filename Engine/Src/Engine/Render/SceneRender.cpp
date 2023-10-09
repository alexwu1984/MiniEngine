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
	struct SceneRenderPrivate
	{
		std::weak_ptr<SceneView> Owner;
		std::shared_ptr<RHIViewPort> MainViewPort;
		std::shared_ptr<PreProcessor> PreProcess;
	};
	
	SceneRender::SceneRender(std::weak_ptr<SceneView> Owner)
	{
		d_ptr = new SceneRenderPrivate();
		d_ptr->Owner = Owner;
	}

	SceneRender::~SceneRender()
	{
		if (GRenderThread)
		{
			GRenderThread->WaitForFinish();
		}
		delete d_ptr;
	}

	std::shared_ptr<SceneView> SceneRender::GetOwner() const
	{
		C_P(SceneRender);
		return d->Owner.lock();
	}

	void SceneRender::InitResource(std::shared_ptr<RHIViewPort> ViewPort)
	{
		C_P(SceneRender);
		d->MainViewPort = ViewPort;
		ENQUEUE_UNIQUE_RENDER_COMMAND(([d](RenderCore::DynamicRHI* RHI){
			if (!d->PreProcess)
			{
				d->PreProcess = std::make_shared<PreProcessor>(RHI);
			}
			d->PreProcess->InitResource();
		}));
	}

	void SceneRender::LoadConfig(const std::wstring& FileName)
	{
		C_P(SceneRender);
		ENQUEUE_UNIQUE_RENDER_COMMAND(([d, FileName](RenderCore::DynamicRHI* RHI) {
			if (d->PreProcess)
			{
				d->PreProcess->LoadConfig(FileName);
			}
		}));
	}

	void SceneRender::Resize(uint32_t InSizeX, uint32_t InSizeY, bool bInIsFullscreen)
	{
		if (InSizeX ==0 || InSizeY == 0)
		{
			return;
		}
		C_P(SceneRender);
		ENQUEUE_UNIQUE_RENDER_COMMAND(([d, InSizeX,InSizeY,bInIsFullscreen](RenderCore::DynamicRHI* RHI) {
			d->MainViewPort->Resize(InSizeX,InSizeY,bInIsFullscreen);
		}));
	}

	void SceneRender::Render()
	{
		std::shared_ptr<RHICommandContext> CommandContext =  GEngine->GetRHI()->GetDefaultCommandContext();
		if (!CommandContext)
		{
			return;
		}
		C_P(SceneRender);
		ENQUEUE_UNIQUE_RENDER_COMMAND(([d, CommandContext](RenderCore::DynamicRHI* RHI) {
			if (d->PreProcess)
			{
				d->PreProcess->Draw(*CommandContext);
			}
			}));

		auto ClearAndSetViewPort = [d](RenderCore::DynamicRHI* RHI) {
			d->MainViewPort->Clear(core::FLinearColor::Gray);
			d->MainViewPort->SetRenderTarget();
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

		auto Present = [d, this](RenderCore::DynamicRHI*) {
			d->MainViewPort->Present();
		};

		ENQUEUE_UNIQUE_RENDER_COMMAND(Present);

	}

	std::shared_ptr<PreProcessor> SceneRender::GetPreProcessor()
	{
		C_P(SceneRender);
		return d->PreProcess;
	}

}


