//Definitions:
//#define NE_DIR_LIGHTS_NUM 1
//#define NE_POINT_LIGHTS_NUM 1
//#define NE_SPOT_LIGHTS_NUM 1
#include "..\LightTypes.hlsli"
#include "Common.hlsl"

#ifdef NE_SHADOWS
#include <..\ShadowCalculations.hlsl>
#endif


#include <..\CommonInput.ps.hlsl>


#ifdef NE_DEFFERED

Texture2D NE_RT_GBuffer_Position : register(t0);
Texture2D NE_RT_GBuffer_Normal : register(t1);
Texture2D NE_RT_GBuffer_AlbedoMetallic : register(t2);
Texture2D NE_RT_GBuffer_RoughnessAO : register(t3);

SamplerState NE_RT_GBuffer_Position_sampler : register(s0);
SamplerState NE_RT_GBuffer_Normal_sampler : register(s1);
SamplerState NE_RT_GBuffer_AlbedoMetallic_sampler : register(s2);
SamplerState NE_RT_GBuffer_RoughnessAO_sampler : register(s3);  //1 Component

//#ifdef IBL_ENABLED
//
//TextureCube NEIBL_IrradianceMap : register(t4);
//TextureCube NEIBL_PrefilterMap : register(t5);
//Texture2D NEIBL_BRDF_LUT : register(t6);
//SamplerState NEIBL_IrradianceMap_sampler : register(s4);
//SamplerState NEIBL_PrefilterMap_sampler : register(s5);
//SamplerState NEIBL_BRDF_LUT_sampler : register(s6);
//
//#endif

#else
Texture2D NEMat_Albedo : register(t0);
Texture2D NEMat_Metallic : register(t1);
Texture2D NEMat_Normal : register(t2); 
Texture2D NEMat_Roughness : register(t3);
Texture2D NEMat_AO : register(t4);

SamplerState NEMat_Albedo_sampler : register(s0);
SamplerState NEMat_Metallic_sampler : register(s1);
SamplerState NEMat_Normal_sampler : register(s2);
SamplerState NEMat_Roughness_sampler : register(s3);
SamplerState NEMat_AO_sampler : register(s4);

#endif


#ifdef IBL_ENABLED

TextureCube NEIBL_IrradianceMap;
TextureCube NEIBL_PrefilterMap;
Texture2D NEIBL_BRDF_LUT;

SamplerState NEIBL_IrradianceMap_sampler;
SamplerState NEIBL_PrefilterMap_sampler;
SamplerState NEIBL_BRDF_LUT_sampler;

#endif

float3 CalculatePBRLight(float3 N, float3 L, float3 V, float3 F0, float3 radiance, float3 albedo, float metalness, float roughness)
{
	float3 H = normalize(V + L);

	// Cook-Torrance BRDF
	float NDF = DistributionGGX(N, H, roughness);
	float G = GeometrySmith(N, V, L, roughness);
	float3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

	float3 numerator = NDF * G * F;
	float denominator = 4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f) + 0.0001; // 0.0001 to prevent divide by zero.
	float3 specular = numerator / max(denominator, 0.001f);

	// kS is equal to Fresnel
	float3 kS = F;
	// for energy conservation, the diffuse and specular light can't
	// be above 1.0 (unless the surface emits light); to preserve this
	// relationship the diffuse component (kD) should equal 1.0 - kS.
	float3 kD = float3(1.0f, 1.0f, 1.0f) - kS;

	// multiply kD by the inverse metalness such that only non-metals 
	// have diffuse lighting, or a linear blend if partly metal (pure metals
	// have no diffuse light).
	kD *= 1.0 - metalness;

	// scale light by NdotL
	float NdotL = max(dot(N, L), 0.0f);

	return float3((kD * albedo / PI + specular) * radiance * NdotL);
}

struct PS_OUTPUT
{
	float4 Color: SV_Target0;
};

// ----------------------------------------------------------------------------
PS_OUTPUT main(PixelInputType input) : SV_TARGET
{
#ifdef NE_DEFFERED
	float3 FragPos = NE_RT_GBuffer_Position.Sample(NE_RT_GBuffer_Position_sampler, input.TexCoord).xyz;
	float3 albedo = pow(NE_RT_GBuffer_AlbedoMetallic.Sample(NE_RT_GBuffer_AlbedoMetallic_sampler, input.TexCoord).rgb, float3(2.2f,2.2f,2.2f));
	float metallic = NE_RT_GBuffer_AlbedoMetallic.Sample(NE_RT_GBuffer_AlbedoMetallic_sampler, input.TexCoord).a;
	float roughness = NE_RT_GBuffer_RoughnessAO.Sample(NE_RT_GBuffer_RoughnessAO_sampler, input.TexCoord).r;
	float ao = NE_RT_GBuffer_RoughnessAO.Sample(NE_RT_GBuffer_RoughnessAO_sampler, input.TexCoord).g;
	float3 N = normalize(NE_RT_GBuffer_Normal.Sample(NE_RT_GBuffer_Normal_sampler, input.TexCoord).rgb);
#else
	float3 FragPos = input.FragPos;
	float3 albedo = pow(NEMat_Albedo.Sample(NEMat_Albedo_sampler, input.TexCoord).xyz, float3(2.2f,2.2f,2.2f));
	float metallic = NEMat_Metallic.Sample(NEMat_Metallic_sampler, input.TexCoord).x;
	float roughness = NEMat_Roughness.Sample(NEMat_Roughness_sampler, input.TexCoord).x;
	float ao = NEMat_AO.Sample(NEMat_AO_sampler, input.TexCoord).x;
	float3 N = NEMat_Normal.Sample(NEMat_Normal_sampler, input.TexCoord).xyz;
	N = normalize(mul(N, 2.0f) - 1.0f);
	N = normalize(mul(N, input.TBN));
#endif

	float3 V = normalize(ViewPos.xyz - FragPos);

	// calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
	// of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)    
	float3 F0 = float3(0.04f, 0.04f, 0.04f);
	F0 = lerp(F0, albedo, metallic);

	// reflectance equation
	float3 Lo = float3(0.0f, 0.0f, 0.0f);
	uint i = 0; //used for iteriation

	// phase 1: dir light
#ifdef NE_DIR_LIGHTS_NUM

#ifdef NE_MAX_DIR_CASTERS

	float dir_Shadow[NE_DIR_LIGHTS_NUM];
	for (i = 0; i < NE_DIR_LIGHTS_NUM; i++)   //initialize array
	{
		dir_Shadow[i] = 0.0f;
	}
	for (i = 0; i < NE_MAX_DIR_CASTERS; i++)   //Shadow enabled light casters first
	{
		dir_Shadow[i] = (1.0f - DirPosShadowCalculation(i, input.DirLight_FragPos[i]));
	}

	for (i = 0; i < NE_DIR_LIGHTS_NUM; i++)  //do lighting + add shadow
	{
		float3 L = normalize(-DirLights[i].Direction.xyz);
		float3 radiance = DirLights[i].Color_Intensity.xyz * DirLights[i].Color_Intensity.w;

		Lo += dir_Shadow[i] * CalculatePBRLight(N, L, V, F0, radiance, albedo, metallic, roughness);
	}

#else  // NO SHADOWS
	for (i = 0; i < NE_DIR_LIGHTS_NUM; i++)
	{
		float3 L = normalize(-DirLights[i].Direction.xyz);
		float3 radiance = DirLights[i].Color_Intensity.xyz * DirLights[i].Color_Intensity.w;

		Lo += CalculatePBRLight(N, L, V, F0, radiance, albedo, metallic, roughness);
	}
#endif  //NE_MAX_DIR_CASTERS

#endif  //NE_DIR_LIGHTS_NUM

	// phase 2: spot light
#ifdef NE_SPOT_LIGHTS_NUM

#ifdef NE_MAX_SPOT_CASTERS 
	float Spot_Shadow[NE_SPOT_LIGHTS_NUM];

	for (i = 0; i < NE_SPOT_LIGHTS_NUM; i++)   //initialize array
	{
		Spot_Shadow[i] = 0.0f;
	}
	for (i = 0; i < NE_MAX_SPOT_CASTERS; i++)   //Shadow enabled light casters first
	{
		Spot_Shadow[i] = (1.0f - SpotShadowCalculation(i, input.SpotLight_FragPos[i]));
	}
	for (i = 0; i < NE_SPOT_LIGHTS_NUM; i++)  //do lighting + add shadow
	{
		float3 L = normalize(SpotLights[i].Position.xyz - FragPos);
		float attenuation = DoQuadraticAttenuation(SpotLights[i].Intensity_Attenuation, SpotLights[i].Position.xyz, FragPos);

		// spotlight intensity
		float theta = dot(L, normalize(-SpotLights[i].Direction.xyz));
		float epsilon = SpotLights[i].InnerCutOf_OuterCutoff.x - SpotLights[i].InnerCutOf_OuterCutoff.y;
		float intensity = saturate((theta - SpotLights[i].InnerCutOf_OuterCutoff.y) / epsilon);

		float3 radiance = SpotLights[i].Color.xyz * attenuation * intensity;

		Lo += Spot_Shadow[i] * CalculatePBRLight(N, L, V, F0, radiance, albedo, metallic, roughness);
	}
#else  //NO SHADOWS
	for (i = 0; i < NE_SPOT_LIGHTS_NUM; i++)  //do lighting + add shadow
	{
		float3 L = normalize(SpotLights[i].Position.xyz - FragPos);
		float attenuation = DoQuadraticAttenuation(SpotLights[i].Intensity_Attenuation, SpotLights[i].Position.xyz, FragPos);

		// spotlight intensity
		float theta = dot(L, normalize(-SpotLights[i].Direction.xyz));
		float epsilon = SpotLights[i].InnerCutOf_OuterCutoff.x - SpotLights[i].InnerCutOf_OuterCutoff.y;
		float intensity = saturate((theta - SpotLights[i].InnerCutOf_OuterCutoff.y) / epsilon);

		float3 radiance = SpotLights[i].Color.xyz * attenuation * intensity;

		Lo += CalculatePBRLight(N, L, V, F0, radiance, albedo, metallic, roughness);
	}
#endif //NE_MAX_SPOT_CASTERS

#endif //NE_SPOT_LIGHTS_NUM


// phase 3: point lights
#ifdef NE_POINT_LIGHTS_NUM   
#ifdef NE_MAX_OMNIDIR_CASTERS

	float point_shadow[NE_POINT_LIGHTS_NUM];
	for (i = 0; i < NE_POINT_LIGHTS_NUM; i++)   //initialize array
	{
		point_shadow[i] = 0.0f;
	}
	for (i = 0; i < NE_MAX_OMNIDIR_CASTERS; i++)   //Shadow enabled light casters first
	{
		point_shadow[i] = (1.0f - OmniDirShadowCalculation(i, FragPos, PointLights[i].Position.xyz, N, PointLights[i].Color_FarPlane.w));
	}
	for (i = 0; i < NE_POINT_LIGHTS_NUM; i++)
	{
		float3 L = normalize(PointLights[i].Position.xyz - FragPos);
		float attenuation = DoQuadraticAttenuation(PointLights[i].Intensity_Attenuation, PointLights[i].Position.xyz, FragPos);
		float3 radiance = PointLights[i].Color_FarPlane.xyz * attenuation;

		Lo += point_shadow[i] * CalculatePBRLight(N, L, V, F0, radiance, albedo, metallic, roughness);
	}
#else  //NO SHADOWS
	for (i = 0; i < NE_POINT_LIGHTS_NUM; i++)
	{
		float3 L = normalize(PointLights[i].Position.xyz - FragPos);
		float attenuation = DoQuadraticAttenuation(PointLights[i].Intensity_Attenuation, PointLights[i].Position.xyz, FragPos);
		float3 radiance = PointLights[i].Color_FarPlane.xyz * attenuation;

		Lo += CalculatePBRLight(N, L, V, F0, radiance, albedo, metallic, roughness);
	}
#endif //NE_MAX_OMNIDIR_CASTERS

#endif  //NE_POINT_LIGHTS_NUM

	PS_OUTPUT output;

///////////////////////////////////////Image based Lighting///////////////////////////////////////
#ifdef IBL_ENABLED
	float3 R = reflect(-V, N);

	// ambient lighting (we now use IBL as the ambient term)
	float3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0f), F0, roughness);

	float3 kS = F;
	float3 kD = 1.0f - kS;
	kD *= 1.0f - metallic;

	float3 irradiance = NEIBL_IrradianceMap.Sample(NEIBL_IrradianceMap_sampler, N).rgb;
	float3 diffuse = irradiance * albedo;
	// sample both the pre-filter map and the BRDF lut and combine them together as per the Split-Sum approximation to get the IBL specular part.
	const float MAX_REFLECTION_LOD = 4.0;
	float3 prefilteredColor = NEIBL_PrefilterMap.SampleLevel(NEIBL_PrefilterMap_sampler, R, roughness * MAX_REFLECTION_LOD).rgb;
	float2 brdf = NEIBL_BRDF_LUT.Sample(NEIBL_BRDF_LUT_sampler, float2(max(dot(N, V), 0.0), roughness)).rg;
	float3 specular = prefilteredColor * (F * brdf.x + brdf.y);

	float3 ambient = (kD * diffuse + specular) * ao;

	float3 color = ambient + Lo;
#else
	float3 ambient = float3(0.03f, 0.03f, 0.03f) * albedo * ao;

	float3 color = ambient + Lo;

#endif

	output.Color = float4(color, 1.0f);

	return output;
}

