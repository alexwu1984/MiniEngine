#include "Render/SceneRendering/SceneViewFamily.h"

namespace Engine
{
	FSceneViewData& FSceneViewFamily::PrimaryView()
	{
		return Views[0];
	}

	const FSceneViewData& FSceneViewFamily::PrimaryView() const
	{
		return Views[0];
	}
}
