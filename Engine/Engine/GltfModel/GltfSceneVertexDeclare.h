#pragma once
#include "core/inc.h"
#include "GltfModel/GltfMeshBuffer.h"
#include "RHI/RHIShdader.h"

namespace Engine
{
	/** GLTF scene VS input layout (FVertexDeclareElementSpec table; matches GLTFVertexFactory.hlsl). */
	void BuildGltfSceneVertexDeclare(uint32_t DeclaredVertexFeatures, RenderCore::RHIVertexDeclare& OutDeclare);

	/** Adds ID_SKINNING_MATRICES when mesh declares skinning. */
	void AppendGltfSceneSkinningShaderMacros(uint32_t DeclaredVertexFeatures, std::vector<RenderCore::RHIShaderMacro>& OutMacros);
}
