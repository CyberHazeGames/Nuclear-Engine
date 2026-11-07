struct PixelInput
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR0;
    float SDF : TEXCOORD1;
};

Texture2D UITexture;
SamplerState UITexture_sampler;

float4 main(PixelInput input) : SV_TARGET
{
    float4 sampled = UITexture.Sample(UITexture_sampler, input.UV);
    // Each quad has a constant SDF value. Evaluate derivatives before branching.
    float smoothing = max(fwidth(sampled.r), 0.001);
    if (input.SDF > 0.5)
    {
        float coverage = smoothstep(0.5 - smoothing, 0.5 + smoothing, sampled.r);
        return float4(input.Color.rgb, input.Color.a * coverage);
    }
    return sampled * input.Color;
}
