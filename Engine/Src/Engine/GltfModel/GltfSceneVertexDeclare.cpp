#include "core/inc.h"
#include "GltfModel/GltfSceneVertexDeclare.h"
#include "RHI/VertexDeclareConfig.h"

namespace Engine
{
	using namespace RenderCore;

	namespace
	{
		// Matches GLTFVertexFactory.hlsl VS_INPUT_SCENE (separable VB: slot N reads stream N).
		const FVertexDeclareElementSpec kGltfSceneCore[] = {
			{ 0, EVertexElementType::VET_Float3 }, // Position  : ATTRIBUTE0
			{ 1, EVertexElementType::VET_Float3 }, // Normal    : ATTRIBUTE1
			{ 2, EVertexElementType::VET_Float2 }, // UV0       : ATTRIBUTE2
			{ 3, EVertexElementType::VET_Float4 }, // Tangent   : ATTRIBUTE3
		};
		const FVertexDeclareElementSpec kGltfSceneSkin[] = {
			{ 4, EVertexElementType::VET_Float4 }, // JointsWeights0 : ATTRIBUTE4
			{ 5, EVertexElementType::VET_Float4 }, // JointsIndices0 : ATTRIBUTE5
		};
	}

	void BuildGltfSceneVertexDeclare(uint32_t DeclaredVertexFeatures, RHIVertexDeclare& OutDeclare)
	{
		BuildVertexDeclareFromSpec(kGltfSceneCore, UE_ARRAY_COUNT(kGltfSceneCore), OutDeclare, false);
		if (DeclaredVertexFeatures & MeshBufferVertexFeatures::Skinning)
			BuildVertexDeclareFromSpec(kGltfSceneSkin, UE_ARRAY_COUNT(kGltfSceneSkin), OutDeclare, true);
	}

	void AppendGltfSceneSkinningShaderMacros(uint32_t DeclaredVertexFeatures, std::vector<RHIShaderMacro>& OutMacros)
	{
		if (DeclaredVertexFeatures & MeshBufferVertexFeatures::Skinning)
			OutMacros.push_back({ "ID_SKINNING_MATRICES", "2" });
	}
}
