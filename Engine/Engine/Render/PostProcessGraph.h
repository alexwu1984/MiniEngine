#pragma once
#include "Render/FRDGBuilder.h"

namespace Engine
{
	using RenderPassResource = FRDGPassResource;
	using RenderPassDesc = FRDGPassDescriptor;
	using PostProcessGraph = FRDGBuilder;
	using RDGCompileParams = FRDGCompileParameters;
}
