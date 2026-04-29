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
#include "Render/RenderTexturePool.h"
#include "core/logger.h"
#include <optional>

using namespace RenderCore;

namespace Engine
{
	namespace
	{
		void ApplyRDGCompileParamsFromJson(const nlohmann::json& Root, FrameGraphCompileParams& Out)
		{
			try
			{
				if (Root.find("RDG") == Root.end() || !Root["RDG"].is_object())
					return;
				const auto& J = Root["RDG"];
				Out.bPassCullingFromSinks = J.value("PassCullingFromSinks", Out.bPassCullingFromSinks);
				Out.bDumpDotToLog = J.value("DumpDotToLog", Out.bDumpDotToLog);
				Out.bLogCompileSummary = J.value("LogCompileSummary", Out.bLogCompileSummary);
			}
			catch (const std::exception&)
			{
			}
		}
	} // namespace

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
		std::atomic_bool IsInit{ false };
		core::FLinearColor Color = core::FLinearColor::Blue;
		FrameGraphCompileParams RDGCompileParams{};
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

	void SceneRender::LoadConfig(const nlohmann::json& Root)
	{
		C_P(SceneRender);
		ApplyRDGCompileParamsFromJson(Root, d->RDGCompileParams);
		ENQUEUE_UNIQUE_RENDER_COMMAND([d, Root](RenderCore::DynamicRHI* RHI) {
			if (d->PreProcess)
				d->PreProcess->LoadConfig(Root);
			if (d->PostProcess)
				d->PostProcess->LoadConfig(Root);
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

	void SceneRender::RenderScene(float DeltaTime)
	{
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
			[d, this, Owner, shadowCasters = std::move(shadowCasters), shadowFrustumBounds = std::move(shadowFrustumBounds),
			 MeshesInfo = std::move(MeshesInfoCopy)](RenderCore::DynamicRHI* RHI)
			{
				std::shared_ptr<RHICommandContext> CommandContext = RHI->GetDefaultCommandContext();
				if (!CommandContext)
					return;

				FrameGraph Graph;
				auto TB = d->TargetBuffer;
				RHI->RHIBeginFrame();

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
						auto SkyCube = IBL->GetSkyLightCubemap();
						d->BackgroundRender->SetTextureCube(SkyCube);
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
					"ImGuiEncode",
					{},
					{},
					[d, this]()
					{
						sigGuiEvent();
						d->MainViewPort->RHIImGuiRenderDrawData();
					}});

				Graph.AddPass(FramePassDesc{
					"RHISubmitAndPresent",
					{},
					{},
					[d]()
					{
						d->MainViewPort->RHISubmitAndPresentFrame();
					}});

				Graph.Execute(d->RDGCompileParams);
				RHI->RHIEndFrame();
			},
			true);
	}

}