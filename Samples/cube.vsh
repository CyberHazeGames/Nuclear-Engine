cbuffer Constants
{
    float4x4 g_WorldViewProj;
};

struct PSInput 
{ 
    float4 Pos : SV_POSITION; 
    float4 Color : COLOR0; 
};

// Vertex shader takes two inputs: vertex position and color.
// By convention, Diligent Engine expects vertex shader inputs to 
// be labeled as ATTRIBn, where n is the attribute number
PSInput main(float3 pos : ATTRIB0, 
             float4 color : ATTRIB1) 
{
    PSInput ps; 
    ps.Pos = mul( float4(pos,1.0), g_WorldViewProj);
    ps.Color = color;
    return ps;
}
