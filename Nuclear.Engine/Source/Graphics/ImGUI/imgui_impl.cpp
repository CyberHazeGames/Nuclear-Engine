// dear imgui: Renderer for Diligent

#include "imgui_impl.h"
#include "Assets/Assets.h"
#include <Graphics/GraphicsModule.h>
#include "Diligent\Graphics\GraphicsTools\interface\GraphicsUtilities.h"
#include <stdio.h>

using namespace Nuclear;
using namespace Diligent;

static RefCntAutoPtr<IBuffer> g_pVB;
static RefCntAutoPtr<IBuffer> g_pIB;

static RefCntAutoPtr<IPipelineState> g_pPSO;
static RefCntAutoPtr<IBuffer> g_pVertexConstantBuffer;
static RefCntAutoPtr<IShaderResourceBinding> g_pSRB;

static RefCntAutoPtr<ITextureView> g_pFontTexture;
static int g_VertexBufferSize = 5000, g_IndexBufferSize = 10000;

struct VERTEX_CONSTANT_BUFFER
{
    float   mvp[4][4];
};

// Render function
// (this used to be set in io.RenderDrawListsFn and called by NuclearEngine::Graphics::ImGui::Render(), but you can now call this directly from your main loop)
void ImGui_Impl_RenderDrawData(ImDrawData* draw_data)
{
    // Avoid rendering when minimized
    if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
        return;

    // Create and grow vertex/index buffers if needed
    if (!g_pVB || g_VertexBufferSize < draw_data->TotalVtxCount)
    {
		if (g_pVB) { g_pVB->Release(); g_pVB = NULL; }
		g_VertexBufferSize = draw_data->TotalVtxCount + 5000;

		BufferDesc VertBuffDesc;
		VertBuffDesc.Usage = USAGE_DYNAMIC;
		VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
		VertBuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
		VertBuffDesc.Size = g_VertexBufferSize * sizeof(ImDrawVert);

		Graphics::GraphicsModule::Get().GetDevice()->CreateBuffer(VertBuffDesc, NULL, &g_pVB);

    }
    if (!g_pIB || g_IndexBufferSize < draw_data->TotalIdxCount)
    {
        if (g_pIB) { g_pIB->Release(); g_pIB = NULL; }
        g_IndexBufferSize = draw_data->TotalIdxCount + 10000;

		BufferDesc IndBuffDesc;
		IndBuffDesc.Usage = USAGE_DYNAMIC;
		IndBuffDesc.BindFlags = BIND_INDEX_BUFFER;
		IndBuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
		IndBuffDesc.Size = g_IndexBufferSize * sizeof(ImDrawIdx);


		Graphics::GraphicsModule::Get().GetDevice()->CreateBuffer(IndBuffDesc, NULL, &g_pIB);
    }

    // Upload vertex/index data into a single contiguous GPU buffer
	PVoid vtx_resource, idx_resource;
	Graphics::GraphicsModule::Get().Get().GetContext()->MapBuffer(g_pVB, MAP_WRITE, MAP_FLAG_DISCARD, (PVoid&)vtx_resource);
	Graphics::GraphicsModule::Get().Get().GetContext()->MapBuffer(g_pIB, MAP_WRITE, MAP_FLAG_DISCARD, (PVoid&)idx_resource);

	ImDrawVert* vtx_dst = (ImDrawVert*)vtx_resource;
	ImDrawIdx* idx_dst = (ImDrawIdx*)idx_resource;
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
		memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
		vtx_dst += cmd_list->VtxBuffer.Size;
		idx_dst += cmd_list->IdxBuffer.Size;
	}

	Graphics::GraphicsModule::Get().Get().GetContext()->UnmapBuffer(g_pVB, MAP_WRITE);
	Graphics::GraphicsModule::Get().Get().GetContext()->UnmapBuffer(g_pIB, MAP_WRITE);

    // Setup orthographic projection matrix into our constant buffer
    // Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
    {
		PVoid mapped_resource;
		Graphics::GraphicsModule::Get().Get().GetContext()->MapBuffer(g_pVertexConstantBuffer, MAP_WRITE, MAP_FLAG_DISCARD, (PVoid&)mapped_resource);

        VERTEX_CONSTANT_BUFFER* constant_buffer = (VERTEX_CONSTANT_BUFFER*)mapped_resource;
        float L = draw_data->DisplayPos.x;
        float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
        float T = draw_data->DisplayPos.y;
        float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
        float mvp[4][4] =
        {
            { 2.0f/(R-L),   0.0f,           0.0f,       0.0f },
            { 0.0f,         2.0f/(T-B),     0.0f,       0.0f },
            { 0.0f,         0.0f,           0.5f,       0.0f },
            { (R+L)/(L-R),  (T+B)/(B-T),    0.5f,       1.0f },
        };
        memcpy(&constant_buffer->mvp, mvp, sizeof(mvp));
		Graphics::GraphicsModule::Get().Get().GetContext()->UnmapBuffer(g_pVertexConstantBuffer, MAP_WRITE);
	}

	// Setup desired DX state
	unsigned int stride = sizeof(ImDrawVert);
	unsigned int offset = 0;

	Graphics::GraphicsModule::Get().Get().GetContext()->SetPipelineState(g_pPSO);

	Diligent::Viewport vp;
	vp.Width = draw_data->DisplaySize.x;
	vp.Height = draw_data->DisplaySize.y;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = vp.TopLeftY = 0;
	Graphics::GraphicsModule::Get().Get().GetContext()->SetViewports(1, &vp,0,0);


	Graphics::GraphicsModule::Get().Get().GetContext()->SetIndexBuffer(g_pIB, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	Graphics::GraphicsModule::Get().Get().GetContext()->SetVertexBuffers(0, 1, &g_pVB, (const Uint64 *)offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);

    // Render command lists
    // (Because we merged all buffers into a single one, we maintain our own offset into them)
    int global_idx_offset = 0;
    int global_vtx_offset = 0;
    ImVec2 clip_off = draw_data->DisplayPos;
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];

			// Apply scissor/clipping rectangle
			const Rect r = { (long)(pcmd->ClipRect.x - clip_off.x), (long)(pcmd->ClipRect.y - clip_off.y), (long)(pcmd->ClipRect.z - clip_off.x), (long)(pcmd->ClipRect.w - clip_off.y) };
			Graphics::GraphicsModule::Get().Get().GetContext()->SetScissorRects(1, &r, 0, 0);

			// Bind texture, Draw
			ITextureView* Tex = static_cast<ITextureView*>(pcmd->TextureId);

			g_pSRB->GetVariableByIndex(SHADER_TYPE_PIXEL, 0)->Set(Tex);
			Graphics::GraphicsModule::Get().Get().GetContext()->CommitShaderResources(g_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

			DrawIndexedAttribs DrawAttrs;
			DrawAttrs.IndexType = sizeof(ImDrawIdx) == 2 ? VT_UINT16 : VT_UINT32;
			DrawAttrs.NumIndices = pcmd->ElemCount;
			DrawAttrs.FirstIndexLocation = pcmd->IdxOffset + global_idx_offset;
			DrawAttrs.BaseVertex = pcmd->VtxOffset + global_vtx_offset;
			Graphics::GraphicsModule::Get().Get().GetContext()->DrawIndexed(DrawAttrs);

		}
        global_idx_offset += cmd_list->IdxBuffer.Size;
        global_vtx_offset += cmd_list->VtxBuffer.Size;
    }
}

static void ImGui_Impl_CreateFontsTexture()
{
    // Build texture atlas
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    // Upload texture to graphics system
    {
		TextureDesc TexDesc;
		TexDesc.Type = RESOURCE_DIM_TEX_2D;
		TexDesc.Width = width;
		TexDesc.Height = height;
		TexDesc.MipLevels = 1;
		TexDesc.ArraySize = 1;
		TexDesc.Usage = USAGE_DEFAULT;
		TexDesc.BindFlags = BIND_SHADER_RESOURCE;
		TexDesc.Format = TEX_FORMAT_RGBA8_UNORM;
		TexDesc.CPUAccessFlags = CPU_ACCESS_NONE;

		TextureSubResData pSubResource;
		pSubResource.pData = pixels;
		pSubResource.Stride = width * 4;

		TextureData TexData;
		TexData.pSubResources = &pSubResource;
		TexData.NumSubresources = 1;
		RefCntAutoPtr<ITexture> mTexture;
		Graphics::GraphicsModule::Get().GetDevice()->CreateTexture(TexDesc, &TexData, &mTexture);

		if (mTexture.RawPtr() != nullptr)
			g_pFontTexture = mTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    }

    // Store our identifier
    io.Fonts->TexID = (ImTextureID)g_pFontTexture.RawPtr();
}

bool ImGui_Impl_CreateDeviceObjects()
{
    ImGui_Impl_InvalidateDeviceObjects();


	GraphicsPipelineStateCreateInfo PSOCreateInfo;
	PSOCreateInfo.PSODesc.Name = "IMGUI_PSO";

	PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
	PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = Graphics::GraphicsModule::Get().GetSwapChain()->GetDesc().ColorBufferFormat;
	PSOCreateInfo.GraphicsPipeline.DSVFormat = Graphics::GraphicsModule::Get().GetSwapChain()->GetDesc().DepthBufferFormat;
	PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;

	//Depth Stencil Desc
	PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = false;
	PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = true;
	PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_ALWAYS;
	PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.StencilEnable = false;
	PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.FrontFace.StencilFailOp = PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.FrontFace.StencilDepthFailOp = PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.FrontFace.StencilPassOp = STENCIL_OP_KEEP;
	PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.FrontFace.StencilFunc = COMPARISON_FUNC_ALWAYS;
	PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.BackFace = PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.FrontFace;
	//Blend State Desc
	PSOCreateInfo.GraphicsPipeline.BlendDesc.AlphaToCoverageEnable = false;
	PSOCreateInfo.GraphicsPipeline.BlendDesc.RenderTargets[0].BlendEnable = true;
	PSOCreateInfo.GraphicsPipeline.BlendDesc.RenderTargets[0].SrcBlend = BLEND_FACTOR_SRC_ALPHA;
	PSOCreateInfo.GraphicsPipeline.BlendDesc.RenderTargets[0].DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;
	PSOCreateInfo.GraphicsPipeline.BlendDesc.RenderTargets[0].BlendOp = BLEND_OPERATION_ADD;
	PSOCreateInfo.GraphicsPipeline.BlendDesc.RenderTargets[0].SrcBlendAlpha = BLEND_FACTOR_INV_SRC_ALPHA;
	PSOCreateInfo.GraphicsPipeline.BlendDesc.RenderTargets[0].DestBlendAlpha = BLEND_FACTOR_ZERO;
	PSOCreateInfo.GraphicsPipeline.BlendDesc.RenderTargets[0].BlendOpAlpha = BLEND_OPERATION_ADD;
	PSOCreateInfo.GraphicsPipeline.BlendDesc.RenderTargets[0].RenderTargetWriteMask = COLOR_MASK_ALL;
	//Rasterizer Desc
	PSOCreateInfo.GraphicsPipeline.RasterizerDesc.FillMode = FILL_MODE_SOLID;
	PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
	PSOCreateInfo.GraphicsPipeline.RasterizerDesc.ScissorEnable = true;
	PSOCreateInfo.GraphicsPipeline.RasterizerDesc.DepthClipEnable = true;

	ShaderCreateInfo ShaderCI;
	ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
	ShaderCI.Desc.UseCombinedTextureSamplers = true;


	// Create vertex shader
	RefCntAutoPtr<IShader> pVS;
	{ 
		const char* vertexShader =
		"cbuffer vertexBuffer : register(b0) \
            {\
            float4x4 ProjectionMatrix; \
            };\
            struct VS_INPUT\
            {\
            float2 pos : ATTRIB0;\
            float2 uv  : ATTRIB1;\
            float4 col : ATTRIB2;\
            };\
            \
            struct PS_INPUT\
            {\
            float4 pos : SV_POSITION;\
            float4 col : COLOR0;\
            float2 uv  : TEXCOORD0;\
            };\
            \
            PS_INPUT main(VS_INPUT input)\
            {\
            PS_INPUT output;\
            output.pos = mul( ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));\
            output.col = input.col;\
            output.uv  = input.uv;\
            return output;\
            }";

		ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
		ShaderCI.EntryPoint = "main";
		ShaderCI.Desc.Name = "IMGUI_VS";
		ShaderCI.Source = vertexShader;
		Graphics::GraphicsModule::Get().GetDevice()->CreateShader(ShaderCI, &pVS);

		CreateUniformBuffer(Graphics::GraphicsModule::Get().GetDevice(), sizeof(Math::Matrix4), "IMGUI_VS_CB", &g_pVertexConstantBuffer);
	}

	// Create pixel shader
	RefCntAutoPtr<IShader> pPS;
	{    
		static const char* pixelShader =
		"struct PS_INPUT\
            {\
            float4 pos : SV_POSITION;\
            float4 col : COLOR0;\
            float2 uv  : TEXCOORD0;\
            };\
            Texture2D Tex0;\
            sampler Tex0_sampler;\
            \
            float4 main(PS_INPUT input) : SV_Target\
            {\
            float4 out_col = input.col * Tex0.Sample(Tex0_sampler, input.uv); \
            return out_col; \
            }";

		ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
		ShaderCI.EntryPoint = "main";
		ShaderCI.Desc.Name = "IMGUI_PS";
		ShaderCI.Source = pixelShader;
		Graphics::GraphicsModule::Get().GetDevice()->CreateShader(ShaderCI, &pPS);
	}

	// Define vertex shader input layout
	LayoutElement LayoutElems[] =
	{
		// Attribute 0 - vertex position
		LayoutElement{0, 0, 2, VT_FLOAT32, False,IM_OFFSETOF(ImDrawVert, pos)},
		// Attribute 1 - texture coordinates
		LayoutElement{1, 0, 2, VT_FLOAT32, False, IM_OFFSETOF(ImDrawVert, uv)},
		// Attribute 2 - color coordinates
		LayoutElement{2, 0, 4, VT_UINT8, True, IM_OFFSETOF(ImDrawVert, col)}
	};

	PSOCreateInfo.pVS = pVS;
	PSOCreateInfo.pPS = pPS;
	PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
	PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElems);

	// Define variable type that will be used by default
	PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

	// Shader variables should typically be mutable, which means they are expected
	// to change on a per-instance basis
	ShaderResourceVariableDesc Vars[] =
	{
		{SHADER_TYPE_PIXEL, "Tex0", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
	};
	PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars;
	PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

	// Define static sampler for g_Texture. Static samplers should be used whenever possible
	SamplerDesc SamLinearClampDesc(FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
		TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP);

	SamLinearClampDesc.ComparisonFunc = COMPARISON_FUNC_ALWAYS;
	ImmutableSamplerDesc StaticSamplers[] =
	{
		{SHADER_TYPE_PIXEL, "Tex0", SamLinearClampDesc}
	};
	PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers = StaticSamplers;
	PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(StaticSamplers);

	Graphics::GraphicsModule::Get().GetDevice()->CreateGraphicsPipelineState(PSOCreateInfo, &g_pPSO);

	g_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "vertexBuffer")->Set(g_pVertexConstantBuffer);

	g_pPSO->CreateShaderResourceBinding(&g_pSRB, true);

    ImGui_Impl_CreateFontsTexture();

    return true;
}

void ImGui_Impl_InvalidateDeviceObjects()
{
    g_pVB.Release();
    g_pIB.Release();
    g_pPSO.Release();
    g_pVertexConstantBuffer.Release();
    g_pSRB.Release();
    g_pFontTexture.Release();
}

bool ImGui_Impl_Init()
{
    // Setup back-end capabilities flags
    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "Nuclear_Engine::DiligentAPI";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.


    return true;
}

void ImGui_Impl_Shutdown()
{
    ImGui_Impl_InvalidateDeviceObjects();
}

void ImGui_Impl_NewFrame()
{
    if (!g_pFontTexture)
       ImGui_Impl_CreateDeviceObjects();
}
