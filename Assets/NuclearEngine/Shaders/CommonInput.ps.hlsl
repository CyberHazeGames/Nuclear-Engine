

#ifndef __COMMONINPUT_HLSL__
#define __COMMONINPUT_HLSL__

#ifdef NE_SHADOWS

cbuffer NEStatic_ActiveShadowCasters
{
    //X: Num of active dir light shadow caster
    //y: Num of active spot light shadow caster
    //z: Num of active point light shadow caster
    //w: padding
    uint4 DirSpotPointActiveCasters;
};
#endif

#endif //__COMMONINPUT_HLSL__

struct PixelInputType
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
#ifndef NE_DEFFERED
    nointerpolation float3 MaterialTint : MATERIAL0;
    nointerpolation float2 MaterialPBR : MATERIAL1;
    float3 Normals : NORMAL0;
    float3 FragPos : TEXCOORD1;
    float3x3 TBN : TANGENT0;
#endif
//Shadows
#ifdef NE_SHADOWS

#ifdef NE_MAX_DIR_CASTERS  
    float4 DirLight_FragPos[NE_MAX_DIR_CASTERS] : TEXCOORD2;
#endif
#ifdef NE_MAX_SPOT_CASTERS  
    float4 SpotLight_FragPos[NE_MAX_SPOT_CASTERS] : COLOR;
#endif


#endif
};
