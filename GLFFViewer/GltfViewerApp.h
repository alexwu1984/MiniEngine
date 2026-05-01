#pragma once
#include "App/WindowsApp.h"
#include "math/vector3.h"
#include <string>
#include <vector>

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
	void ReloadScene(int32_t NewIndex);
	void HideActor(const std::wstring& Name);
	void ShowActor(const std::wstring& Name);
private:
	std::shared_ptr<Engine::GltfActor> AGltfModel;
	int32_t SelIndex = 0;
	int32_t PendingModelIndex = -1;
	std::wstring ProcessDir;
	std::vector<std::wstring> ModelFiles;
	std::vector<std::string> ModelLabelsUtf8;
	math::Vector3 mDirectLight{ 0,0,1 };
	std::shared_ptr<Engine::SimplePostProcessor> _Demo;
	float xHDRRotate{ 0.f };
	float yHDRRotate{ 0.f };
};