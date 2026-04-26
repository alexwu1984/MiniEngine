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
#include "Render/FrameGraph.h"
#include "Render/GBuffer.h"
#include "Render/Shadow/ShadowRenderPass.h"
#include "Render/SimplePostProcessor.h"
#include "Render/RenderTexturePool.h"
#include "core/logger.h"
#include <mutex>
#include <optional>
#include <unordered_map>

using namespace RenderCore;

namespace
{
	std::mutex GExclusiveFullscreenEffectRegistryMutex;
	std::unordered_map<std::string, Engine::SceneRender::ExclusiveFullscreenEffectFactory> GExclusiveFullscreenEffectFactories;

	std::shared_ptr<Engine::SimplePostProcessor> CreateExclusiveFullscreenEffectFromRegistry(const std::string& Id, DynamicRHI* RHI)
	{
		std::lock_guard<std::mutex> Lock(GExclusiveFullscreenEffectRegistryMutex);
		const auto It = GExclusiveFullscreenEffectFactories.find(Id);
		if (It == GExclusiveFullscreenEffectFactories.end())
			return nullptr;
		return It->second(RHI);
	}
}

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
		std::shared_ptr<ShadowRenderPass> ShadowRender;
		std::shared_ptr<SimplePostProcessor> SimplePostProc; // Optional fullscreen sample pass (bypasses main scene graph).
		std::atomic_bool IsInit{ false };
		core::FLinearColor Color = core::FLinearColor::Blue;
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
			d->TargetBuffer->InitDefaultSceneTargets(Size.cx, Size.cy);

			if (!d->ShadowRender)
			{
				d->ShadowRender = std::make_shared<ShadowRenderPass>(RHI);
			}
			d->ShadowRender->InitResource();
			d->IsInit = true;
			});
	}

	void SceneRender::RegisterExclusiveFullscreenEffect(std::string Id, ExclusiveFullscreenEffectFactory Factory)
	{
		std::lock_guard<std::mutex> Lock(GExclusiveFullscreenEffectRegistryMutex);
		GExclusiveFullscreenEffectFactories[std::move(Id)] = std::move(Factory);
	}

	void SceneRender::UnregisterExclusiveFullscreenEffect(const std::string& Id)
	{
		std::lock_guard<std::mutex> Lock(GExclusiveFullscreenEffectRegistryMutex);
		GExclusiveFullscreenEffectFactories.erase(Id);
	}

	void SceneRender::LoadConfig(const nlohmann::json& Root)
	{
		C_P(SceneRender);
		std::optional<std::string> ExclusiveFullscreenEffectId;
		try
		{
			if (Root.find("Evn") != Root.end() && Root["Evn"].is_object())
			{
				const auto& Evn = Root["Evn"];
				const nlohmann::json* EffectNode = nullptr;
				if (Evn.find("ExclusiveFullscreenPostEffect") != Evn.end())
					EffectNode = &Evn["ExclusiveFullscreenPostEffect"];
				else if (Evn.find("ExclusiveFullscreenPostDemo") != Evn.end())
					EffectNode = &Evn["ExclusiveFullscreenPostDemo"];
				if (EffectNode)
				{
					ExclusiveFullscreenEffectId = std::string{};
					const auto& V = *EffectNode;
					if (V.is_string())
						*ExclusiveFullscreenEffectId = V.get<std::string>();
					else if (V.is_boolean() && V.get<bool>())
						*ExclusiveFullscreenEffectId = "PostProcessorDemo";
				}
			}
		}
		catch (const std::exception&)
		{
		}

		ENQUEUE_UNIQUE_RENDER_COMMAND([d, Root, ExclusiveFullscreenEffectId](RenderCore::DynamicRHI* RHI) {
			if (d->PreProcess)
				d->PreProcess->LoadConfig(Root);
			if (d->PostProcess)
				d->PostProcess->LoadConfig(Root);

			if (!ExclusiveFullscreenEffectId.has_value())
				return;

			if (ExclusiveFullscreenEffectId->empty())
			{
				d->SimplePostProc.reset();
				return;
			}

			std::shared_ptr<SimplePostProcessor> Effect = CreateExclusiveFullscreenEffectFromRegistry(*ExclusiveFullscreenEffectId, RHI);
			if (Effect)
			{
				Effect->InitResource();
				d->SimplePostProc = std::move(Effect);
			}
			else
			{
				core::LOG(core::log_war, L"ExclusiveFullscreenPostEffect: no factory registered for id \"%S\"",
						  ExclusiveFullscreenEffectId->c_str());
				d->SimplePostProc.reset();
			}
		});
	}

	void SceneRender::SetBackgroundColor(const core::FLinearColor& Color)
	{
		C_P(SceneRender);
		d->Color = Color;
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
				d->TargetBuffer->InitDefaultSceneTargets(InSizeX, InSizeY);
			if (d->PostProcess)
				d->PostProcess->InvalidateTransientResources();
		});
	}

	void SceneRender::Render(float DeltaTime)
	{
		C_P(SceneRender);
		if (d->SimplePostProc)
			RenderSimple(DeltaTime);
		else
			RenderScene(DeltaTime);
	}

	void SceneRender::SetIBLRotate(float x, float y)
	{
		C_P(SceneRender);
		d->BackgroundRender->SetRotate(x, y);
		d->BaseRender->SetIBLRotate(x, y);
	}

	std::shared_ptr<PreProcessor> SceneRender::GetPreProcessor() const
	{
		C_P(const SceneRender);
		return d->PreProcess;
	}

	std::shared_ptr<PostProcessor> SceneRender::GetPostProcessor() const
	{
		C_P(const SceneRender);
		return d->PostProcess;
	}

	bool SceneRender::UsesTemporalAAProjectionJitter() const
	{
		C_P(const SceneRender);
		if (!d->PostProcess)
			return false;
		return d->PostProcess->GetPostProcessorAAType() == EPostProcessorAAType::TAA;
	}

	std::shared_ptr<ShadowRenderPass> SceneRender::GetShadowRenderPass() const
	{
		C_P(const SceneRender);
		return d->ShadowRender;
	}

	std::shared_ptr<RHIViewPort> SceneRender::GetViewPort() const
	{
		C_P(const SceneRender);
		return d->MainViewPort;
	}

	void SceneRender::SetSamplePostProcessor(std::shared_ptr<SimplePostProcessor> postProcessor)
	{
		C_P(SceneRender);
		d->SimplePostProc = postProcessor;
	}

	void SceneRender::RenderSimple(float DeltaTime)
	{
		C_P(SceneRender);

		ENQUEUE_UNIQUE_RENDER_COMMAND(
			[d, this, DeltaTime](RenderCore::DynamicRHI* RHI)
			{
				RenderTexturePool::Get().BeginFrame();
				d->MainViewPort->Clear(d->Color);
				d->MainViewPort->Prepare();
				int32_t width = GEngine->GetAppWindow()->GetWidth();
				int32_t height = GEngine->GetAppWindow()->GetHeight();
				RHI->GetDefaultCommandContext()->SetViewPort(0, 0, width, height);

				d->SimplePostProc->Draw(*RHI->GetDefaultCommandContext(), d->MainViewPort, DeltaTime);
				sigGuiEvent();
				d->MainViewPort->Present();
				RenderTexturePool::Get().EndFrame();
			},
			true);
	}

	void SceneRender::RenderScene(float DeltaTime)
	{
		std::shared_ptr<RHICommandContext> CommandContext = GEngine->GetRHI()->GetDefaultCommandContext();
		if (!CommandContext)
			return;
		C_P(SceneRender);
		std::shared_ptr<SceneView> Owner = GetOwner();
		if (!d->IsInit || !Owner || !Owner->GetMainCamera())
			return;

		d->MeshesInfo.clear();
		std::vector<GltfSceneMeshInfo> shadowFrustumBounds;
		std::vector<GltfSceneMeshInfo> shadowCasters;

		const auto& Actors = Owner->GetAllActors();
		for (const auto& ActorItem : Actors)
		{
			if (ActorItem->GetState() != Actor::EActive || !ActorItem->IsVisible())
				continue;
			auto Components = std::move(ActorItem->GetComponents<GltfMeshComponent>());
			for (auto& ComponentItem : Components)
			{
				GltfSceneMeshInfo SceneMeshInfo;
				if (!ComponentItem->GatherMesh(SceneMeshInfo, Owner->GetMainCamera()))
					continue;
				shadowFrustumBounds.push_back(SceneMeshInfo);
				if (ActorItem->IsProjectShadow())
					shadowCasters.push_back(SceneMeshInfo);
			}
		}

		for (const auto& ActorItem : Actors)
		{
			if (ActorItem->GetState() == Actor::EActive && ActorItem->IsVisible())
			{
				auto Components = std::move(ActorItem->GetComponents<GltfMeshComponent>());
				for (auto& ComponentItem : Components)
				{
					GltfSceneMeshInfo SceneMeshInfo;
					if (ComponentItem->GatherMesh(SceneMeshInfo, Owner->GetMainCamera()))
						d->MeshesInfo.push_back(SceneMeshInfo);
				}
			}
		}

		std::vector<GltfSceneMeshInfo> MeshesInfoCopy = d->MeshesInfo;

		ENQUEUE_UNIQUE_RENDER_COMMAND(
			[d, this, CommandContext, Owner, shadowCasters = std::move(shadowCasters), shadowFrustumBounds = std::move(shadowFrustumBounds),
			 MeshesInfo = std::move(MeshesInfoCopy)](RenderCore::DynamicRHI* RHI)
			{
				FrameGraph Graph;
				auto TB = d->TargetBuffer;
				RenderTexturePool::Get().BeginFrame();

				Graph.AddPass(FramePassDesc{
					"PreProcess",
					{},
					{},
					[d, CommandContext]()
					{
						if (d->PreProcess)
							d->PreProcess->Draw(*CommandContext);
					}});

				if (!shadowCasters.empty())
				{
					Graph.AddPass(FramePassDesc{
						"Shadow",
						{},
						{},
						[d, CommandContext, shadowCasters, shadowFrustumBounds, Owner]()
						{ d->ShadowRender->Render(shadowCasters, shadowFrustumBounds, *CommandContext, Owner); }});
				}

				Graph.AddPass(FramePassDesc{
					"ClearGBufferAndBackground",
					{},
					{
						{ "SceneColor", [TB]() { return TB->GetSceneColor(); } },
						{ "MotionVector", [TB]() { return TB->GetMotionVector(); } },
						{ "Normal", [TB]() { return TB->GetNormalBuffer(); } },
						{ "Emissive", [TB]() { return TB->GetEmissiveBuffer(); } },
						{ "MetallicRoughness", [TB]() { return TB->GetMetallicRoughnessBuffer(); } },
						{ "Depth", [TB]() { return TB->GetDepth(); } },
					},
					[d, RHI]()
					{
						d->MainViewPort->SetRenderTarget();
						d->MainViewPort->Clear(d->Color);
						d->MainViewPort->Prepare();
						int32_t width = GEngine->GetAppWindow()->GetWidth();
						int32_t height = GEngine->GetAppWindow()->GetHeight();
						RHI->GetDefaultCommandContext()->SetViewPort(0, 0, width, height);

						std::vector<std::shared_ptr<RenderCore::RHITexture2D>> Targets = {
							d->TargetBuffer->GetSceneColor(), d->TargetBuffer->GetMotionVector(), d->TargetBuffer->GetNormalBuffer(),
							d->TargetBuffer->GetEmissiveBuffer(), d->TargetBuffer->GetMetallicRoughnessBuffer()};
						RHI->GetDefaultCommandContext()->Clear(Targets, d->TargetBuffer->GetDepth(), core::FLinearColor::Black, 1.f, 0);

						auto IBL = d->PreProcess->GetIBLRender();
						auto EvnCube = IBL->GetEvnCube();
						d->BackgroundRender->SetTextureCube(EvnCube);
						d->BackgroundRender->Render(*RHI->GetDefaultCommandContext(), Targets, d->TargetBuffer->GetDepth());
					}});

				Graph.AddPass(FramePassDesc{
					"Geometry",
					{
						{ "SceneColor", [TB]() { return TB->GetSceneColor(); } },
						{ "MotionVector", [TB]() { return TB->GetMotionVector(); } },
						{ "Normal", [TB]() { return TB->GetNormalBuffer(); } },
						{ "Emissive", [TB]() { return TB->GetEmissiveBuffer(); } },
						{ "MetallicRoughness", [TB]() { return TB->GetMetallicRoughnessBuffer(); } },
						{ "Depth", [TB]() { return TB->GetDepth(); } },
					},
					{
						{ "SceneColor", [TB]() { return TB->GetSceneColor(); } },
						{ "MotionVector", [TB]() { return TB->GetMotionVector(); } },
						{ "Normal", [TB]() { return TB->GetNormalBuffer(); } },
						{ "Emissive", [TB]() { return TB->GetEmissiveBuffer(); } },
						{ "MetallicRoughness", [TB]() { return TB->GetMetallicRoughnessBuffer(); } },
						{ "Depth", [TB]() { return TB->GetDepth(); } },
					},
					[d, RHI, MeshesInfo, Owner]()
					{
						if (MeshesInfo.size())
							d->BaseRender->Render(RHI, MeshesInfo, Owner, d->TargetBuffer);
					}});

				d->PostProcess->AddFramePasses(Graph, *RHI->GetDefaultCommandContext(), d->TargetBuffer, d->MainViewPort, Owner->GetMainCamera());

				Graph.AddPass(FramePassDesc{
					"Present",
					{},
					{},
					[d, this]()
					{
						sigGuiEvent();
						d->MainViewPort->Present();
					}});

				Graph.Execute();
				RenderTexturePool::Get().EndFrame();
			},
			true);
	}

}