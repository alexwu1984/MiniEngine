#pragma once
#include "App/WindowsApp.h"
#include "math/vector3.h"
#include "Renderer.h"

class Tex2DRender;
class ViewerApp : public Engine::WindowApplication
{
public:
	ViewerApp();
	virtual ~ViewerApp();

	virtual bool Init() override;
	virtual void ShutDown() override;

private:
	void BuildRenderScene();
	void BuildExerciseScene();

private:
	std::shared_ptr<Tex2DRender> _Demo;
	Renderer m_render;
};