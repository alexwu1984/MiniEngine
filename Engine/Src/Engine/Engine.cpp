#include "Engine/Engine.h"
#include "RHI/DynamicRHI.h"
#include "core/commandline.h"

namespace Engine
{
	MainEngine* GEngine = nullptr;

	struct MainEngineP
	{
		MainEngineP()
		{
			_DynamicRHI = RenderCore::PlatformCreateDynamicRHI(RenderCore::RHIAPIType::E_D3D11);
		}
		std::shared_ptr<RenderCore::DynamicRHI> _DynamicRHI;
	};

	MainEngine::MainEngine()
		:Data(std::make_shared<MainEngineP>())
	{
		GEngine = this;
	}

	MainEngine::~MainEngine()
	{

	}

	void MainEngine::Init()
	{
		if (Data->_DynamicRHI)
		{
			Data->_DynamicRHI->Init();
		}
	}

	std::shared_ptr<RenderCore::DynamicRHI> MainEngine::GetRHI() const
	{
		return Data->_DynamicRHI;
	}

}