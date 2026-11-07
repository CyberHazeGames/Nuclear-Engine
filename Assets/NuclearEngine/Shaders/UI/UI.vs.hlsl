struct VertexInput
{
    float2 Position : ATTRIB0;
    float2 UV : ATTRIB1;
    float4 Color : ATTRIB2;
    float SDF : ATTRIB3;
};

struct PixelInput
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR0;
    float SDF : TEXCOORD1;
};

PixelInput main(VertexInput input)
{
    PixelInput output;
    output.Position = float4(input.Position, 0.0, 1.0);
    output.UV = input.UV;
    output.Color = input.Color;
    output.SDF = input.SDF;
    return output;
}
