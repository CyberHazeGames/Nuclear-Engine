float4 main(uint vertex : SV_VertexID) : SV_POSITION
{
    // One oversized triangle covers the target without a vertex buffer.
    float2 position = float2((vertex << 1) & 2, vertex & 2);
    return float4(position * float2(2, -2) + float2(-1, 1), 0, 1);
}
