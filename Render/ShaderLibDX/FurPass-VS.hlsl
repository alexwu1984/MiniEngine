#include "FurVertexFactory.hlsl"

VS_OUTPUT_FUR MainVS(VS_INPUT_FUR input, uint InstanceId : SV_InstanceID)
{
	return furVertexFactory(input, InstanceId);
}
