#include "App/WindowsApp.h"

namespace Engine
{
	class GltfActor;
}

class GltfViewApp : public Engine::WindowApplication
{
public:
	GltfViewApp();
	virtual ~GltfViewApp();

	virtual bool Init() override;
	virtual void ShutDown() override;

private:
	void HideActor(const std::wstring& Name);
	void ShowActor(const std::wstring& Name);
private:
	std::shared_ptr<Engine::GltfActor> AGltfModel;
	int32_t SelIndex = 0;
};