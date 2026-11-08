#include "Editor/SelectionConstants.hlsli"

float4 main(float3 position : ATTRIB0) : SV_POSITION
{
    return mul(ModelViewProjection, float4(position, 1));
}
