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
    float2 pixelX = ddx(input.UV);
    float2 pixelY = ddy(input.UV);
    if (input.SDF < 0.5)
        return UITexture.SampleGrad(UITexture_sampler, input.UV, pixelX, pixelY) * input.Color;

    // Integrate the contour over one physical pixel. Narrow stems and counters can
    // cross only part of a pixel even when its center is wholly inside or outside.
    float coverage = 0;
    [unroll]
    for (int y = 0; y < 4; ++y)
    {
        [unroll]
        for (int x = 0; x < 4; ++x)
        {
            float2 offset = (float2(x, y) + 0.5) / 4.0 - 0.5;
            float distance = UITexture.SampleLevel(UITexture_sampler, input.UV + offset.x * pixelX + offset.y * pixelY, 0).r;
            float subpixelBand = max(0.25 * fwidth(distance), 1.0 / 255.0);
            coverage += saturate((distance - 0.5) / subpixelBand + 0.5);
        }
    }
    return float4(input.Color.rgb, input.Color.a * (coverage / 16.0));
}
