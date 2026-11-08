// Keep aligned with EditorSelectionRenderer::Resources::Constants (96 bytes).
cbuffer SelectionConstants
{
    float4x4 ModelViewProjection;
    uint ObjectId;
    uint SelectedId;
    uint TargetWidth;
    uint TargetHeight;
    uint OutlineRadius;
    uint3 SelectionPadding;
};
