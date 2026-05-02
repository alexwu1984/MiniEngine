#pragma once
#include "App/WindowsApp.h"
#include "math/vector3.h"

namespace Engine
{
	class GltfActor;
	class SimplePostProcessor;
	class World;
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
	std::shared_ptr<Engine::SimplePostProcessor> _Demo;
	float xHDRRotate{ 16.875f };
	float yHDRRotate{ -114.375f };
};