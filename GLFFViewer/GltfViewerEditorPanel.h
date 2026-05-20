#pragma once
#include "core/inc.h"

namespace Engine
{
	class FWorldSceneRender;
	class World;
}

/** GLTF Viewer main ImGui panel (model, lights, viewport debug, GBuffer, performance). */
class GltfViewerEditorPanel
{
public:
	void SetModelSelection(std::vector<std::string> ModelLabelsUtf8, int32_t* SelectedIndex,
						   std::atomic<int32_t>* PendingModelIndex);

	void Bind(Engine::FWorldSceneRender& SceneRender);
	void Unbind(Engine::FWorldSceneRender& SceneRender);

private:
	void Draw();
	void DrawModelCombo(Engine::World& Scene);
	void DrawViewportSection(Engine::World& Scene);
	void DrawDirectionalCsmSection(Engine::World& Scene);
	void DrawSceneLightsSection(Engine::World& Scene);
	void DrawModelTransformSection(Engine::World& Scene);
	void DrawGBufferSection();
	void DrawPerformanceSection();

	std::vector<std::string> ModelLabelsUtf8{};
	int32_t* SelectedIndex = nullptr;
	std::atomic<int32_t>* PendingModelIndex = nullptr;
	bool bBound = false;
};
