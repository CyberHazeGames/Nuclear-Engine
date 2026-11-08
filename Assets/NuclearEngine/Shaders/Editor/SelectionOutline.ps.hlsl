#include "Editor/SelectionConstants.hlsli"

Texture2D<uint> SceneIds;

bool IsSelected(int2 position)
{
    if (any(position < 0) || position.x >= int(TargetWidth) || position.y >= int(TargetHeight))
        return false;
    return SceneIds.Load(int3(position, 0)) == SelectedId;
}

float4 main(float4 position : SV_POSITION) : SV_TARGET
{
    int2 pixel = int2(position.xy);
    if (SelectedId == 0)
        discard;

    uint centerId = SceneIds.Load(int3(pixel, 0));
    bool inside = centerId == SelectedId;
    // Keep an inner border against other meshes; expand only into background
    // so neither foreground occluders nor neighboring geometry get painted over.
    if (!inside && centerId != 0)
        discard;

    int radius = int(OutlineRadius);
    bool edge = false;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
            edge = edge || (inside != IsSelected(pixel + int2(x, y) * radius));
    }
    if (!edge)
        discard;

    return float4(1, 0.8, 0, 1);
}
