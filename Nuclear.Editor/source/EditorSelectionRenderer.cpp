#include <EditorSelectionRenderer.h>
#include <Assets/Material.h>
#include <Assets/Mesh.h>
#include <Components/CameraComponent.h>
#include <Components/EntityInfoComponent.h>
#include <Components/MeshComponent.h>
#include <Core/Scene.h>
#include <Diligent/Graphics/GraphicsTools/interface/MapHelper.hpp>
#include <Graphics/GraphicsModule.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace Nuclear::Editor
{
	using namespace Diligent;

	struct EditorSelectionRenderer::Resources
	{
		struct Constants
		{
			Math::Matrix4 ModelViewProjection = Math::Matrix4(1);
			Uint32 ObjectId = 0;
			Uint32 SelectedId = 0;
			Uint32 Width = 0;
			Uint32 Height = 0;
			Uint32 Radius = 1;
			Uint32 Padding[3]{};
		};
		static_assert(sizeof(Constants) == 96);

		RefCntAutoPtr<IPipelineState> IdPipeline;
		RefCntAutoPtr<IPipelineState> OutlinePipeline;
		RefCntAutoPtr<IShaderResourceBinding> IdBinding;
		RefCntAutoPtr<IShaderResourceBinding> OutlineBinding;
		RefCntAutoPtr<IBuffer> Buffer;
		RefCntAutoPtr<ITexture> Ids;
		RefCntAutoPtr<ITexture> Depth;
		RefCntAutoPtr<ITexture> Readback;
		std::vector<entt::entity> Entities;
	};

	EditorSelectionRenderer::EditorSelectionRenderer() : pResources(std::make_unique<Resources>()) {}
	EditorSelectionRenderer::~EditorSelectionRenderer() = default;

	void EditorSelectionRenderer::Shutdown()
	{
		*pResources = Resources{};
	}

	bool EditorSelectionRenderer::Initialize(Components::CameraComponent& camera)
	{
		Shutdown();
		auto* device = Graphics::GraphicsModule::Get().GetDevice();
		if (!device || !camera.GetColorRT().GetRTV())
			return false;

		auto& resources = *pResources;
		BufferDesc buffer;
		buffer.Name = "Editor selection constants";
		buffer.Size = sizeof(Resources::Constants);
		buffer.Usage = USAGE_DYNAMIC;
		buffer.BindFlags = BIND_UNIFORM_BUFFER;
		buffer.CPUAccessFlags = CPU_ACCESS_WRITE;
		device->CreateBuffer(buffer, nullptr, &resources.Buffer);
		if (!resources.Buffer)
			return false;

		auto loadShader = [](const char* path, SHADER_TYPE stage) {
			RefCntAutoPtr<IShader> result;
			Graphics::ShaderObjectCreationDesc desc;
			desc.mPath = path;
			desc.mName = path;
			desc.mType = stage;
			Graphics::GraphicsModule::Get().CreateShader(&result, desc);
			return result;
		};
		auto idVertex = loadShader("@NuclearAssets@/Shaders/Editor/Selection.vs.hlsl", SHADER_TYPE_VERTEX);
		auto idPixel = loadShader("@NuclearAssets@/Shaders/Editor/Selection.ps.hlsl", SHADER_TYPE_PIXEL);
		auto outlineVertex = loadShader("@NuclearAssets@/Shaders/Editor/SelectionOutline.vs.hlsl", SHADER_TYPE_VERTEX);
		auto outlinePixel = loadShader("@NuclearAssets@/Shaders/Editor/SelectionOutline.ps.hlsl", SHADER_TYPE_PIXEL);
		if (!idVertex || !idPixel || !outlineVertex || !outlinePixel)
			return false;

		GraphicsPipelineStateCreateInfo id;
		id.PSODesc.Name = "Editor visible mesh IDs";
		id.GraphicsPipeline.NumRenderTargets = 1;
		id.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_R32_UINT;
		id.GraphicsPipeline.DSVFormat = TEX_FORMAT_D32_FLOAT;
		id.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		id.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
		id.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;
		id.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = true;
		id.GraphicsPipeline.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_LESS;
		const LayoutElement layout[] = {
			{0, 0, 3, VT_FLOAT32, false, offsetof(Assets::Mesh::Vertex, Position), sizeof(Assets::Mesh::Vertex)}};
		id.GraphicsPipeline.InputLayout.LayoutElements = layout;
		id.GraphicsPipeline.InputLayout.NumElements = 1;
		id.pVS = idVertex;
		id.pPS = idPixel;
		device->CreateGraphicsPipelineState(id, &resources.IdPipeline);

		GraphicsPipelineStateCreateInfo outline;
		outline.PSODesc.Name = "Editor yellow selection outline";
		outline.GraphicsPipeline.NumRenderTargets = 1;
		outline.GraphicsPipeline.RTVFormats[0] = camera.GetColorRT().GetRTV()->GetTexture()->GetDesc().Format;
		outline.GraphicsPipeline.DSVFormat = TEX_FORMAT_UNKNOWN;
		outline.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		outline.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
		outline.GraphicsPipeline.DepthStencilDesc.DepthEnable = false;
		outline.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = false;
		outline.pVS = outlineVertex;
		outline.pPS = outlinePixel;
		const ShaderResourceVariableDesc variables[] = {
			{SHADER_TYPE_PIXEL, "SceneIds", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};
		outline.PSODesc.ResourceLayout.Variables = variables;
		outline.PSODesc.ResourceLayout.NumVariables = 1;
		device->CreateGraphicsPipelineState(outline, &resources.OutlinePipeline);
		if (!resources.IdPipeline || !resources.OutlinePipeline)
			return false;

		resources.IdPipeline->GetStaticVariableByName(SHADER_TYPE_VERTEX, "SelectionConstants")->Set(resources.Buffer);
		resources.IdPipeline->GetStaticVariableByName(SHADER_TYPE_PIXEL, "SelectionConstants")->Set(resources.Buffer);
		resources.OutlinePipeline->GetStaticVariableByName(SHADER_TYPE_PIXEL, "SelectionConstants")->Set(resources.Buffer);
		resources.IdPipeline->CreateShaderResourceBinding(&resources.IdBinding, true);
		resources.OutlinePipeline->CreateShaderResourceBinding(&resources.OutlineBinding, true);

		TextureDesc readback;
		readback.Name = "Editor selection click readback";
		readback.Type = RESOURCE_DIM_TEX_2D;
		readback.Width = readback.Height = 1;
		readback.MipLevels = 1;
		readback.Format = TEX_FORMAT_R32_UINT;
		readback.Usage = USAGE_STAGING;
		readback.CPUAccessFlags = CPU_ACCESS_READ;
		device->CreateTexture(readback, nullptr, &resources.Readback);
		return resources.IdBinding && resources.OutlineBinding && resources.Readback;
	}

	bool EditorSelectionRenderer::Resize(unsigned width, unsigned height)
	{
		auto& resources = *pResources;
		if (resources.Ids && resources.Ids->GetDesc().Width == width && resources.Ids->GetDesc().Height == height)
			return true;

		TextureDesc desc;
		desc.Name = "Editor visible entity IDs";
		desc.Type = RESOURCE_DIM_TEX_2D;
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = 1;
		desc.Format = TEX_FORMAT_R32_UINT;
		desc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
		RefCntAutoPtr<ITexture> ids;
		RefCntAutoPtr<ITexture> depth;
		auto* device = Graphics::GraphicsModule::Get().GetDevice();
		device->CreateTexture(desc, nullptr, &ids);
		desc.Name = "Editor selection depth";
		desc.Format = TEX_FORMAT_D32_FLOAT;
		desc.BindFlags = BIND_DEPTH_STENCIL;
		device->CreateTexture(desc, nullptr, &depth);
		if (!ids || !depth)
			return false;
		resources.Ids = ids;
		resources.Depth = depth;
		return true;
	}

	bool EditorSelectionRenderer::RenderIds(Components::CameraComponent& camera)
	{
		auto& resources = *pResources;
		resources.Entities.clear();
		resources.Entities.push_back(entt::null);
		const auto size = camera.GetColorRT().GetDimensions();
		if (!resources.IdPipeline || !size.x || !size.y || !Resize(size.x, size.y))
			return false;

		auto* context = Graphics::GraphicsModule::Get().GetContext();
		auto* target = resources.Ids->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
		auto* depth = resources.Depth->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
		context->SetRenderTargets(1, &target, depth, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		const float clear[] = {0, 0, 0, 0};
		context->ClearRenderTarget(target, clear, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		context->ClearDepthStencil(depth, CLEAR_DEPTH_FLAG, 1, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		Viewport viewport;
		viewport.Width = float(size.x);
		viewport.Height = float(size.y);
		context->SetViewports(1, &viewport, size.x, size.y);
		context->SetPipelineState(resources.IdPipeline);

		auto& registry = Core::Scene::Get().GetRegistry();
		for (auto entity : registry.view<Components::MeshComponent, Components::EntityInfoComponent>())
		{
			auto& component = registry.get<Components::MeshComponent>(entity);
			component.Update();
			auto* mesh = component.GetMesh();
			if (!component.GetEnableRendering() || !component.GetRenderQueue() || !mesh ||
				mesh->GetState() != Assets::IAsset::State::Created)
				continue;
			if (resources.Entities.size() >= std::numeric_limits<Uint32>::max())
				return false;

			auto& transform = registry.get<Components::EntityInfoComponent>(entity).mTransform;
			transform.Update();
			Resources::Constants constants;
			constants.ModelViewProjection = camera.GetProjectionMatrix() * camera.mCameraData.View * transform.GetWorldMatrix();
			constants.ObjectId = static_cast<Uint32>(resources.Entities.size());
			resources.Entities.push_back(entity);
			{
				MapHelper<Resources::Constants> mapped(context, resources.Buffer, MAP_WRITE, MAP_FLAG_DISCARD);
				*mapped = constants;
			}
			context->CommitShaderResources(resources.IdBinding, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
			for (auto& submesh : mesh->mSubMeshes)
			{
				if (!submesh.mVB || !submesh.mIB || !submesh.mIndicesCount)
					continue;
				IBuffer* vertex = submesh.mVB;
				const Uint64 offset = 0;
				context->SetVertexBuffers(0, 1, &vertex, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
					SET_VERTEX_BUFFERS_FLAG_RESET);
				context->SetIndexBuffer(submesh.mIB, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
				DrawIndexedAttribs draw;
				draw.IndexType = VT_UINT32;
				draw.NumIndices = submesh.mIndicesCount;
				draw.Flags = DRAW_FLAG_VERIFY_ALL;
				context->DrawIndexed(draw);
			}
		}
		return true;
	}

	entt::entity EditorSelectionRenderer::Pick(Components::CameraComponent& camera, UI::Rect viewport, UI::Vec2 position)
	{
		if (viewport.Width <= 0 || viewport.Height <= 0 || !viewport.Contains(position) ||
			!std::isfinite(position.x) || !std::isfinite(position.y) || !RenderIds(camera))
			return entt::null;

		auto& resources = *pResources;
		const auto& desc = resources.Ids->GetDesc();
		const auto x = std::min(desc.Width - 1, Uint32((position.x - viewport.x) / viewport.Width * desc.Width));
		const auto y = std::min(desc.Height - 1, Uint32((position.y - viewport.y) / viewport.Height * desc.Height));
		const Box sourceBox(x, x + 1, y, y + 1);
		auto* context = Graphics::GraphicsModule::Get().GetContext();
		context->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
		CopyTextureAttribs copy;
		copy.pSrcTexture = resources.Ids;
		copy.pSrcBox = &sourceBox;
		copy.pDstTexture = resources.Readback;
		copy.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
		copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
		context->CopyTexture(copy);
		context->WaitForIdle();
		MappedTextureSubresource mapped;
		context->MapTextureSubresource(resources.Readback, 0, 0, MAP_READ, MAP_FLAG_NONE, nullptr, mapped);
		if (!mapped.pData)
			return entt::null;
		Uint32 id = 0;
		std::memcpy(&id, mapped.pData, sizeof(id));
		context->UnmapTextureSubresource(resources.Readback, 0, 0);
		const auto entity = id < resources.Entities.size() ? resources.Entities[id] : entt::null;
		return Core::Scene::Get().GetRegistry().valid(entity) ? entity : entt::null;
	}

	bool EditorSelectionRenderer::Render(Components::CameraComponent& camera, entt::entity selected, float scale)
	{
		auto& registry = Core::Scene::Get().GetRegistry();
		if (selected == entt::null || !registry.valid(selected))
			return true;
		auto* selectedMesh = registry.try_get<Components::MeshComponent>(selected);
		if (!selectedMesh || !selectedMesh->GetEnableRendering())
			return true;
		if (!std::isfinite(scale) || scale <= 0 || !RenderIds(camera))
			return false;

		auto& resources = *pResources;
		const auto found = std::find(resources.Entities.begin(), resources.Entities.end(), selected);
		if (found == resources.Entities.end())
			return true;
		Resources::Constants constants;
		constants.SelectedId = static_cast<Uint32>(found - resources.Entities.begin());
		constants.Width = resources.Ids->GetDesc().Width;
		constants.Height = resources.Ids->GetDesc().Height;
		constants.Radius = static_cast<Uint32>(std::clamp(std::ceil(scale), 1.f, 8.f));
		auto* context = Graphics::GraphicsModule::Get().GetContext();
		{
			MapHelper<Resources::Constants> mapped(context, resources.Buffer, MAP_WRITE, MAP_FLAG_DISCARD);
			*mapped = constants;
		}
		auto* target = camera.GetColorRT().GetRTV();
		context->SetRenderTargets(1, &target, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		context->SetPipelineState(resources.OutlinePipeline);
		resources.OutlineBinding->GetVariableByName(SHADER_TYPE_PIXEL, "SceneIds")->Set(
			resources.Ids->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
		context->CommitShaderResources(resources.OutlineBinding, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		DrawAttribs draw;
		draw.NumVertices = 3;
		draw.Flags = DRAW_FLAG_VERIFY_ALL;
		context->Draw(draw);
		return true;
	}
}
