#include "GLTFVertexFactory.hlsl"


//--------------------------------------------------------------------------------------
// MainVS
//--------------------------------------------------------------------------------------
VS_OUTPUT_SCENE MainVS(VS_INPUT_SCENE input, uint InstanceId : SV_InstanceID)
{
    return gltfVertexFactory(input, InstanceId);
}
