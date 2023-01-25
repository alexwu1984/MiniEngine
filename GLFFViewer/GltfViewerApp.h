#include "App/WindowsApp.h"

namespace Engine
{
	class GltfOrbitActor;
}

class GltfViewApp : public Engine::WindowApplication
{
public:
	GltfViewApp();
	virtual ~GltfViewApp();

	virtual bool Init() override;
	virtual void ShutDown() override;
private:
	std::shared_ptr<Engine::GltfOrbitActor> GltfActor;
};