#include "Render/SceneRender.h"
#include "Scene/SceneView.h"
#include "RHI/RHICommandContext.h"
#include "Scene/Actor.h"
#include "Scene/Component.h"
#include "Scene/GltfMeshComponent.h"
#include "RHI/RHIViewPort.h"
#include "Thread/RenderThread.h"
#include "Engine.h"
#include "RHI/DynamicRHI.h"
#include "App/AppWindow.h"
#include "Render/PreProcessor.h"
#include "GltfModel/GltfMesh.h"
#include "Render/BasePassRender.h"
#include "Render/CubeBackground.h"
#include "Render/IBLRender.h"
#include "Render/PostProcessor.h"
#include "Render/GBuffer.h"

using namespace RenderCore;

namespace Engine
{
	struct SceneRenderPrivate
	{
		std::weak_ptr<SceneView> Owner;
		std::shared_ptr<RHIViewPort> MainViewPort;
		std::shared_ptr<PreProcessor> PreProcess;
		std::shared_ptr<PostProcessor> PostProcess;
		std::shared_ptr<BasePassRender> BaseRender;
		std::shared_ptr<CubeBackground> BackgroundRender;
		std::shared_ptr<GBuffer> TargetBuffer;
		std::vector<GltfSceneMeshInfo> MeshesInfo;
	};
	
	SceneRender::SceneRender(std::weak_ptr<SceneView> Owner)
		:d_ptr(new SceneRenderPrivate())
	{
		C_P(SceneRender);
		d->Owner = Owner;
		d->BaseRender = std::make_shared<BasePassRender>();
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

		ENQUEUE_UNIQUE_RENDER_COMMAND([d](RenderCore::DynamicRHI* RHI) {
			if (!d->PreProcess)
			{
				d->PreProcess = std::make_shared<PreProcessor>(RHI);
			}
			d->PreProcess->InitResource();
			if (!d->PostProcess)
			{
				d->PostProcess = std::make_shared<PostProcessor>(RHI);
			}
			d->PostProcess->InitResource();

			if (!d->BackgroundRender)
			{
				d->BackgroundRender = std::make_shared<CubeBackground>(RHI);
			}
			d->BackgroundRender->InitResource();

			if (!d->TargetBuffer)
			{
				d->TargetBuffer = std::make_shared<GBuffer>(RHI);
			}
			auto Size = d->MainViewPort->GetSize();
			d->TargetBuffer->InitResource(static_cast<GBufferFlagBits>(GBufferFlagBits::GBUFFER_DEPTH | GBufferFlagBits::GBUFFER_MOTION_VECTORS | GBufferFlagBits::GBUFFER_SCENE_COLOR | GBufferFlagBits::GBUFFER_NORMAL_BUFFER),
				Size.cx, Size.cy);
			});
	}

	void SceneRender::LoadConfig(const std::wstring& FileName)
	{
		C_P(SceneRender);
		ENQUEUE_UNIQUE_RENDER_COMMAND([d, FileName](RenderCore::DynamicRHI* RHI) {
			if (d->PreProcess)
			{
				d->PreProcess->LoadConfig(FileName);
			}
		});
	}

	void SceneRender::Resize(uint32_t InSizeX, uint32_t InSizeY, bool bInIsFullscreen)
	{
		if (InSizeX ==0 || InSizeY == 0)
		{
			return;
		}
		C_P(SceneRender); 
		ENQUEUE_UNIQUE_RENDER_COMMAND([d, InSizeX, InSizeY, bInIsFullscreen](RenderCore::DynamicRHI* RHI) {
			d->MainViewPort->Resize(InSizeX, InSizeY, bInIsFullscreen);
			if (d->TargetBuffer)
			{
				d->TargetBuffer->InitResource(static_cast<GBufferFlagBits>(GBufferFlagBits::GBUFFER_DEPTH | GBufferFlagBits::GBUFFER_MOTION_VECTORS | GBufferFlagBits::GBUFFER_SCENE_COLOR | GBufferFlagBits::GBUFFER_NORMAL_BUFFER), InSizeX, InSizeY);
			}
			if (d->PostProcess)
			{
				d->PostProcess->InitResource();
			}
			
		});
	}

	void SceneRender::Render()
	{
		std::shared_ptr<RHICommandContext> CommandContext =  GEngine->GetRHI()->GetDefaultCommandContext();
		if (!CommandContext)
		{
			return;
		}
		C_P(SceneRender);

		ENQUEUE_UNIQUE_RENDER_COMMAND([d, CommandContext](RenderCore::DynamicRHI* RHI) {
			if (d->PreProcess)
			{
				d->PreProcess->Draw(*CommandContext);
			}
			});

		ENQUEUE_UNIQUE_RENDER_COMMAND([d](RenderCore::DynamicRHI* RHI) {
			d->MainViewPort->Clear(core::FLinearColor::Gray);
			d->MainViewPort->SetRenderTarget();
			int32_t width = GEngine->GetAppWindow()->GetWidth();
			int32_t height = GEngine->GetAppWindow()->GetHeight();
			RHI->GetDefaultCommandContext()->SetViewPort(0, 0, width, height);

			std::vector < std::shared_ptr<RenderCore::RHITexture2D> > Targets = { d->TargetBuffer->GetSceneColor(),d->TargetBuffer->GetMotionVector(),d->TargetBuffer->GetNormalBuffer() };
			RHI->GetDefaultCommandContext()->SetRenderTarget(Targets, d->TargetBuffer->GetDepth());
			RHI->GetDefaultCommandContext()->Clear(Targets, d->TargetBuffer->GetDepth(), core::FLinearColor::Black, 1.f, 0);
			});

		ENQUEUE_UNIQUE_RENDER_COMMAND([d](RenderCore::DynamicRHI* RHI) {
			auto IBL = d->PreProcess->GetIBLRender();
			auto EvnCube = IBL->GetEvnCube();
			d->BackgroundRender->SetTextureCube(EvnCube);
			d->BackgroundRender->Render(*RHI->GetDefaultCommandContext());
		});
		
		d->MeshesInfo.clear();
		const auto& Actors = GetOwner()->GetAllActors();
		for (const auto& ActorItem : Actors)
		{
			if (ActorItem->GetState() == Actor::EActive)
			{
				auto Components = std::move(ActorItem->GetComponents<GltfMeshComponent>());
				for (auto& ComponentItem : Components)
				{
					GltfSceneMeshInfo SceneMeshInfo;
					if (ComponentItem->GatherMesh(SceneMeshInfo, GetOwner()->GetMainCamera()))
					{
						d->MeshesInfo.push_back(SceneMeshInfo);
					}
				}
			}
		}

		if (d->MeshesInfo.size())
		{
			d->BaseRender->Render(d->MeshesInfo, *CommandContext, GetOwner()->GetMainCamera());
		}

		ENQUEUE_UNIQUE_RENDER_COMMAND([d, this](RenderCore::DynamicRHI* RHI)
			{
				d->MainViewPort->SetRenderTarget();
				if (d->PostProcess)
				{
					d->PostProcess->Draw(*RHI->GetDefaultCommandContext(), d->TargetBuffer);
				}
		});


		ENQUEUE_UNIQUE_RENDER_COMMAND([d, this](RenderCore::DynamicRHI*) {
			d->MainViewPort->Present();
		});

	}

	std::shared_ptr<PreProcessor> SceneRender::GetPreProcessor()
	{
		C_P(SceneRender);
		return d->PreProcess;
	}

}


