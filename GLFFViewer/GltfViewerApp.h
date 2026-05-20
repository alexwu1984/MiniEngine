#pragma once
#include "core/inc.h"
#include "App/WindowsApp.h"

class GltfViewerEditorPanel;

namespace Engine
{
	class GltfActor;
	class SimplePostProcessor;
}

class GltfViewApp : public Engine::WindowApplication
{
public:
	GltfViewApp();
	virtual ~GltfViewApp();

	virtual bool Init() override;
	virtual void ShutDown() override;

private:
	void BuildModelList();
	void BindImGuiToSceneRender();
	void FlushPendingModelReload();
	void ReloadScene(int32_t NewIndex);
	void HideActor(const std::wstring& Name);
	void ShowActor(const std::wstring& Name);
private:
	std::shared_ptr<Engine::GltfActor> AGltfModel;
	int32_t SelIndex = 0;
	std::atomic<int32_t> PendingModelIndex{-1};
	std::wstring ProcessDir;
	std::vector<std::wstring> ModelFiles;
	std::vector<std::string> ModelLabelsUtf8;
	std::unique_ptr<GltfViewerEditorPanel> EditorPanel;
	std::shared_ptr<Engine::SimplePostProcessor> _Demo;
};
