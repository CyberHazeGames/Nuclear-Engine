#include <Diligent/Graphics/GraphicsTools/interface/MapHelper.hpp>
#include <Graphics/GraphicsModule.h>
#include <Platform/FileSystem.h>
#include <UI/UIRenderer.h>
#include <cmath>
#include <cstring>
#include <limits>

namespace Nuclear::UI
{
	using namespace Diligent;

	struct UIRenderer::Resources
	{
		struct Vertex
		{
			Vec2 Position;
			Vec2 UV;
			Color Tint;
			float SDF;
		};
		RefCntAutoPtr<IPipelineState> Pipeline;
		RefCntAutoPtr<IShaderResourceBinding> Binding;
		RefCntAutoPtr<IBuffer> Vertices;
		RefCntAutoPtr<IBuffer> Indices;
		RefCntAutoPtr<ITextureView> White;
		size_t Capacity = 0;
	};

	UIRenderer::UIRenderer() : pResources(std::make_unique<Resources>()) {}
	UIRenderer::~UIRenderer()
	{
		Shutdown();
	}
	void UIRenderer::Shutdown()
	{
		*pResources = Resources{};
	}

	bool UIRenderer::Initialize()
	{
		Shutdown();
		auto& graphics = Graphics::GraphicsModule::Get();
		auto* device = graphics.GetDevice();
		auto* swapChain = graphics.GetSwapChain();
		if (!device || !swapChain)
			return false;
		auto& resources = *pResources;
		RefCntAutoPtr<IShader> vertexShader;
		RefCntAutoPtr<IShader> pixelShader;
		ShaderCreateInfo shader;
		shader.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
		shader.EntryPoint = "main";
		shader.Desc.UseCombinedTextureSamplers = true;
		shader.Desc.Name = "Nuclear UI vertex shader";
		shader.Desc.ShaderType = SHADER_TYPE_VERTEX;
		auto vertexSource =
			Platform::FileSystem::Get().LoadShader("@NuclearAssets@/Shaders/UI/UI.vs.hlsl", {}, {}, true);
		shader.Source = vertexSource.c_str();
		device->CreateShader(shader, &vertexShader);
		shader.Desc.Name = "Nuclear UI pixel shader";
		shader.Desc.ShaderType = SHADER_TYPE_PIXEL;
		auto pixelSource =
			Platform::FileSystem::Get().LoadShader("@NuclearAssets@/Shaders/UI/UI.ps.hlsl", {}, {}, true);
		shader.Source = pixelSource.c_str();
		device->CreateShader(shader, &pixelShader);
		if (!vertexShader || !pixelShader)
			return false;

		GraphicsPipelineStateCreateInfo pipeline;
		pipeline.PSODesc.Name = "Nuclear retained UI";
		auto& desc = pipeline.GraphicsPipeline;
		desc.NumRenderTargets = 1;
		desc.RTVFormats[0] = swapChain->GetDesc().ColorBufferFormat;
		desc.DSVFormat = TEX_FORMAT_UNKNOWN;
		desc.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		desc.RasterizerDesc.CullMode = CULL_MODE_NONE;
		desc.RasterizerDesc.ScissorEnable = true;
		desc.DepthStencilDesc.DepthEnable = false;
		desc.DepthStencilDesc.DepthWriteEnable = false;
		auto& blend = desc.BlendDesc.RenderTargets[0];
		blend.BlendEnable = true;
		blend.SrcBlend = BLEND_FACTOR_SRC_ALPHA;
		blend.DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;
		blend.BlendOp = BLEND_OPERATION_ADD;
		blend.SrcBlendAlpha = BLEND_FACTOR_ONE;
		blend.DestBlendAlpha = BLEND_FACTOR_INV_SRC_ALPHA;
		blend.BlendOpAlpha = BLEND_OPERATION_ADD;
		const LayoutElement layout[] = {
			{0, 0, 2, VT_FLOAT32, false, offsetof(Resources::Vertex, Position), sizeof(Resources::Vertex)},
			{1, 0, 2, VT_FLOAT32, false, offsetof(Resources::Vertex, UV), sizeof(Resources::Vertex)},
			{2, 0, 4, VT_FLOAT32, false, offsetof(Resources::Vertex, Tint), sizeof(Resources::Vertex)},
			{3, 0, 1, VT_FLOAT32, false, offsetof(Resources::Vertex, SDF), sizeof(Resources::Vertex)}};
		desc.InputLayout.LayoutElements = layout;
		desc.InputLayout.NumElements = 4;
		pipeline.pVS = vertexShader;
		pipeline.pPS = pixelShader;
		const ShaderResourceVariableDesc variable[] = {
			{SHADER_TYPE_PIXEL, "UITexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};
		const SamplerDesc sampler(FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, TEXTURE_ADDRESS_CLAMP,
								  TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP);
		const ImmutableSamplerDesc immutable[] = {{SHADER_TYPE_PIXEL, "UITexture", sampler}};
		pipeline.PSODesc.ResourceLayout.Variables = variable;
		pipeline.PSODesc.ResourceLayout.NumVariables = 1;
		pipeline.PSODesc.ResourceLayout.ImmutableSamplers = immutable;
		pipeline.PSODesc.ResourceLayout.NumImmutableSamplers = 1;
		device->CreateGraphicsPipelineState(pipeline, &resources.Pipeline);
		if (!resources.Pipeline)
			return false;
		resources.Pipeline->CreateShaderResourceBinding(&resources.Binding, true);

		TextureDesc texture;
		texture.Name = "UI solid brush";
		texture.Type = RESOURCE_DIM_TEX_2D;
		texture.Width = texture.Height = 1;
		texture.Format = TEX_FORMAT_RGBA8_UNORM;
		texture.BindFlags = BIND_SHADER_RESOURCE;
		texture.Usage = USAGE_IMMUTABLE;
		const Uint32 white = 0xFFFFFFFF;
		TextureSubResData pixels;
		pixels.pData = &white;
		pixels.Stride = sizeof(white);
		TextureData data;
		data.pSubResources = &pixels;
		data.NumSubresources = 1;
		RefCntAutoPtr<ITexture> whiteTexture;
		device->CreateTexture(texture, &data, &whiteTexture);
		if (whiteTexture)
			resources.White = whiteTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
		return resources.Binding && resources.White;
	}

	void UIRenderer::Render(const DrawList& list, Vec2 pixelSize, float scale, ITextureView* target)
	{
		auto& resources = *pResources;
		if (!resources.Pipeline || list.Quads.empty() || pixelSize.x <= 0 || pixelSize.y <= 0 || scale <= 0)
			return;
		if (list.Quads.size() > std::numeric_limits<Uint32>::max() / 6)
			return;
		auto& graphics = Graphics::GraphicsModule::Get();
		auto* context = graphics.GetContext();
		if (resources.Capacity < list.Quads.size())
		{
			resources.Vertices.Release();
			resources.Indices.Release();
			resources.Capacity = list.Quads.size() + 256;
			BufferDesc buffer;
			buffer.Name = "UI vertices";
			buffer.Usage = USAGE_DYNAMIC;
			buffer.CPUAccessFlags = CPU_ACCESS_WRITE;
			buffer.BindFlags = BIND_VERTEX_BUFFER;
			buffer.Size = resources.Capacity * 4 * sizeof(Resources::Vertex);
			graphics.GetDevice()->CreateBuffer(buffer, nullptr, &resources.Vertices);
			buffer.Name = "UI indices";
			buffer.BindFlags = BIND_INDEX_BUFFER;
			buffer.Size = resources.Capacity * 6 * sizeof(Uint32);
			graphics.GetDevice()->CreateBuffer(buffer, nullptr, &resources.Indices);
			if (!resources.Vertices || !resources.Indices)
			{
				resources.Capacity = 0;
				return;
			}
		}
		{
			MapHelper<Resources::Vertex> vertices(context, resources.Vertices, MAP_WRITE, MAP_FLAG_DISCARD);
			MapHelper<Uint32> indices(context, resources.Indices, MAP_WRITE, MAP_FLAG_DISCARD);
			for (size_t index = 0; index < list.Quads.size(); ++index)
			{
				const auto& quad = list.Quads[index];
				const float left = quad.Bounds.x * scale / pixelSize.x * 2 - 1;
				const float top = 1 - quad.Bounds.y * scale / pixelSize.y * 2;
				const float right = left + quad.Bounds.Width * scale / pixelSize.x * 2;
				const float bottom = top - quad.Bounds.Height * scale / pixelSize.y * 2;
				const float u = quad.UV.x;
				const float v = quad.UV.y;
				const float uEnd = u + quad.UV.Width;
				const float vEnd = v + quad.UV.Height;
				const float sdf = quad.SDF ? 1.0f : 0.0f;
				const size_t base = index * 4;
				vertices[base] = {{left, top}, {u, v}, quad.Tint, sdf};
				vertices[base + 1] = {{right, top}, {uEnd, v}, quad.Tint, sdf};
				vertices[base + 2] = {{right, bottom}, {uEnd, vEnd}, quad.Tint, sdf};
				vertices[base + 3] = {{left, bottom}, {u, vEnd}, quad.Tint, sdf};
				const Uint32 offsets[] = {0, 1, 2, 0, 2, 3};
				for (size_t element = 0; element < 6; ++element)
					indices[index * 6 + element] = static_cast<Uint32>(base) + offsets[element];
			}
		}

		if (!target)
			target = graphics.GetSwapChain()->GetCurrentBackBufferRTV();
		context->SetRenderTargets(1, &target, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		context->SetPipelineState(resources.Pipeline);
		Viewport viewport;
		viewport.Width = pixelSize.x;
		viewport.Height = pixelSize.y;
		context->SetViewports(1, &viewport, static_cast<Uint32>(pixelSize.x), static_cast<Uint32>(pixelSize.y));
		IBuffer* vertexBuffer = resources.Vertices;
		const Uint64 offset = 0;
		context->SetVertexBuffers(0, 1, &vertexBuffer, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
								  SET_VERTEX_BUFFERS_FLAG_RESET);
		context->SetIndexBuffer(resources.Indices, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		auto scissor = [scale, pixelSize](Rect clip)
		{
			Rect pixels = {clip.x * scale, clip.y * scale, clip.Width * scale, clip.Height * scale};
			pixels = pixels.Intersect({0, 0, pixelSize.x, pixelSize.y});
			return Diligent::Rect{static_cast<Int32>(std::floor(pixels.x)), static_cast<Int32>(std::floor(pixels.y)),
								  static_cast<Int32>(std::ceil(pixels.x + pixels.Width)),
								  static_cast<Int32>(std::ceil(pixels.y + pixels.Height))};
		};
		for (size_t begin = 0; begin < list.Quads.size();)
		{
			const auto& first = list.Quads[begin];
			auto* textureView = first.pTexture ? first.pTexture : resources.White.RawPtr();
			const auto clip = scissor(first.Clip);
			size_t end = begin + 1;
			while (end < list.Quads.size())
			{
				const auto& next = list.Quads[end];
				const auto nextClip = scissor(next.Clip);
				if ((next.pTexture ? next.pTexture : resources.White.RawPtr()) != textureView ||
					clip.left != nextClip.left || clip.top != nextClip.top || clip.right != nextClip.right ||
					clip.bottom != nextClip.bottom)
					break;
				++end;
			}
			if (clip.right > clip.left && clip.bottom > clip.top)
			{
				context->SetScissorRects(1, &clip, static_cast<Uint32>(pixelSize.x), static_cast<Uint32>(pixelSize.y));
				resources.Binding->GetVariableByName(SHADER_TYPE_PIXEL, "UITexture")->Set(textureView);
				context->CommitShaderResources(resources.Binding, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
				DrawIndexedAttribs draw;
				draw.NumIndices = static_cast<Uint32>((end - begin) * 6);
				draw.IndexType = VT_UINT32;
				draw.FirstIndexLocation = static_cast<Uint32>(begin * 6);
				draw.Flags = DRAW_FLAG_VERIFY_ALL;
				context->DrawIndexed(draw);
			}
			begin = end;
		}
	}
} // namespace Nuclear::UI
