#include "GLTFVertexFactory.hlsl"


//--------------------------------------------------------------------------------------
// MainVS
//--------------------------------------------------------------------------------------
VS_OUTPUT_SCENE MainVS(VS_INPUT_SCENE input)
{
    VS_OUTPUT_SCENE Output = gltfVertexFactoryForLight(input);

    return Output;
}
