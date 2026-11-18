/*
Defines:
NE_DEFFERED

*/

#include <CommonInput.vs.hlsl>
#include <CommonInput.ps.hlsl>


PixelInputType main(VertexInputType input)
{
    PixelInputType output;

#ifndef NE_DEFFERED

    float4 FinalPos = float4(input.Position.xyz, 1.0f);
    float4 FinalNorm = float4(input.Normals.xyz, 0.0f);

#if defined(NE_ANIMATION) || defined(ANIMATION)
    float4 weightedPos = 0, weightedNormal = 0;
    float totalWeight = 0;
    for (int i = 0; i < MAX_BONE_INFLUENCE; ++i)
    {
        int bone = input.BoneIDs[i];
        float weight = input.Weights[i];
        if (bone < 0 || bone >= MAX_BONES || weight <= 0) continue;
        weightedPos += weight * mul(BoneTransforms[bone], FinalPos);
        weightedNormal += weight * mul(BoneTransforms[bone], FinalNorm);
        totalWeight += weight;
    }
    if (totalWeight > 0)
    {
        FinalPos = weightedPos / totalWeight;
        FinalNorm = weightedNormal / totalWeight;
    }
#endif

    output.MaterialTint = SurfaceTint.rgb;
    output.MaterialPBR = SurfacePBR.xy;
    output.Position = mul(ModelViewProjection, FinalPos);
    output.FragPos = mul(Model, FinalPos).xyz;
    output.Normals = normalize(mul(Model, FinalNorm).xyz);

    float3 T = normalize(mul(input.Tangents, (float3x3)Model));
    float3 B = normalize(mul(input.Bitangents, (float3x3)Model));
    float3 N = output.Normals;
    // TBN must form a right handed coord system.
   // Some models have symetric UVs. Check and fix.
    if (dot(cross(N, T), B) < 0.0)
    {
        T = T * -1.0;
    }
    output.TBN = float3x3(T, B, N);
	
#ifdef NE_SHADOWS

#ifdef NE_MAX_DIR_CASTERS

    for (uint i0 = 0; i0 < NE_MAX_DIR_CASTERS; i0++)
    {
        output.DirLight_FragPos[i0] = mul(Dir_LightSpace[i0], float4(output.FragPos, 1.0f));
    }
#endif

#ifdef NE_MAX_SPOT_CASTERS

    for (uint i1 = 0; i1 < NE_MAX_SPOT_CASTERS; i1++)
    {
        output.SpotLight_FragPos[i1] = mul(Spot_LightSpace[i1], float4(output.FragPos, 1.0f));
    }
#endif


#endif //NE_SHADOWS

#else

    output.Position = input.Position;
#endif
    output.TexCoord = input.TexCoord;

    return output;
}