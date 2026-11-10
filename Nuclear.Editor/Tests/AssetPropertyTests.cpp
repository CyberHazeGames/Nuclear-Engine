#include "AssetPropertyTests.h"
#include <NuclearEditor.h>
#include <EntityEditor.h>
#include <EditorAssetProperty.h>
#include <EditorAssetPicker.h>
#include <EditorComponentButton.h>
#include <EditorHistory.h>
#include <EditorColorPickerWindow.h>
#include <Fallbacks/FallbacksModule.h>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace Nuclear::Editor::Tests
{
	namespace
	{
		void Check(bool condition, const char* message)
		{
			if (!condition) throw std::runtime_error(message);
		}
		template<class T, class Predicate> T* Find(UI::UIContext& context, Predicate predicate)
		{
			UI::Widget marker;
			for (UI::WidgetId id = 1; id < marker.GetId(); ++id)
				if (auto* widget = dynamic_cast<T*>(context.Find(id)); widget && predicate(*widget)) return widget;
			return nullptr;
		}
		UI::TextBlock* Text(UI::UIContext& context, const std::string& text)
		{
			return Find<UI::TextBlock>(context, [&](const auto& label) { return label.IsVisible() && label.GetText() == text; });
		}
		void Click(UI::Widget& widget)
		{
			auto& context = *widget.GetContext();
			context.Layout();
			for (auto* parent = widget.GetParent(); parent; parent = parent->GetParent())
				if (auto* scroll = dynamic_cast<UI::ScrollBox*>(parent))
				{
					scroll->ScrollIntoView(widget);
					context.Layout();
				}
			const auto bounds = widget.GetBounds();
			Platform::Event event;
			event.mType = Platform::EventType::MouseButton;
			event.mMouseX = int((bounds.x + bounds.Width * .5f) * context.GetScale());
			event.mMouseY = int((bounds.y + bounds.Height * .5f) * context.GetScale());
			event.mButton = MOUSE_BUTTON_LEFT;
			event.mPressed = true;
			context.ProcessEvent(event);
			event.mPressed = false;
			context.ProcessEvent(event);
		}
		void Action(UI::UIContext& context, const std::string& title)
		{
			auto* text = Find<UI::TextBlock>(context, [&](const auto& label) {
				return label.IsVisible() && label.GetText() == title && dynamic_cast<UI::Button*>(label.GetParent());
			});
			Check(text && dynamic_cast<UI::Button*>(text->GetParent()), ("Button exists: " + title).c_str());
			Click(*text->GetParent());
		}
		void Open(UI::UIContext& context, const char* property)
		{
			auto* title = Text(context, property);
			Check(title != nullptr, "Typed asset property exists");
			auto* row = title->GetParent()->GetParent();
			auto* button = Find<UI::Button>(context, [&](const auto& button) { return button.GetParent() == row; });
			Check(button != nullptr, "Asset property has an ellipsis button");
			Click(*button);
			Check(Find<EditorAssetPicker>(context, [](const auto&) { return true; }) != nullptr, "Ellipsis opens asset picker");
		}
		size_t Readback(Diligent::ITexture* source, const char* path, bool centralCoverage = false)
		{
			using namespace Diligent;
			auto& graphics = Graphics::GraphicsModule::Get();
			auto desc = source->GetDesc();
			desc.Name = "Inspector preview test readback";
			desc.Usage = USAGE_STAGING;
			desc.BindFlags = BIND_NONE;
			desc.CPUAccessFlags = CPU_ACCESS_READ;
			RefCntAutoPtr<ITexture> staging;
			graphics.GetDevice()->CreateTexture(desc, nullptr, &staging);
			graphics.GetContext()->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
			CopyTextureAttribs copy;
			copy.pSrcTexture = source;
			copy.pDstTexture = staging;
			copy.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
			copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
			graphics.GetContext()->CopyTexture(copy);
			graphics.GetContext()->WaitForIdle();
			MappedTextureSubresource mapped;
			graphics.GetContext()->MapTextureSubresource(staging, 0, 0, MAP_READ, MAP_FLAG_NONE, nullptr, mapped);
			Check(mapped.pData != nullptr, "Preview pixels can be read back");
			std::ofstream file(path, std::ios::binary);
			file << "P6\n" << desc.Width << ' ' << desc.Height << "\n255\n";
			size_t bright = 0;
			for (Uint32 y = 0; y < desc.Height; ++y)
				for (Uint32 x = 0; x < desc.Width; ++x)
				{
					const auto* pixel = static_cast<const unsigned char*>(mapped.pData) + y * mapped.Stride + x * 4;
					file.write(reinterpret_cast<const char*>(pixel), 3);
					const bool central = x > desc.Width * .3f && x < desc.Width * .7f &&
						y > desc.Height * .3f && y < desc.Height * .7f;
					if ((!centralCoverage || central) && (pixel[0] > 50 || pixel[1] > 50 || pixel[2] > 50)) ++bright;
				}
			graphics.GetContext()->UnmapTextureSubresource(staging, 0, 0);
			return bright;
		}
	}

	void RunAssetPropertyTests()
	{
		NuclearEditor editor;
		editor.Load();
		auto frame = [&]() {
			auto& engine = Core::Engine::Get();
			engine.BeginFrame(); editor.Update(1.f / 60); editor.Render(1.f / 60); engine.EndFrame();
		};
		frame();
		auto& library = Assets::AssetLibrary::Get();
		auto& registry = Core::Scene::Get().GetRegistry();
		const auto mainEntity = *registry.view<Components::MeshComponent>().begin();
		auto* defaultMaterial = registry.get<Components::MeshComponent>(mainEntity).GetMaterial();
		auto& cube = library.mImportedMeshes.AddAsset();
		cube.SetName("Inspector Cube");
		Assets::Mesh::CreateCube(&cube, 20, 20, 20);
		auto& sphere = library.mImportedMeshes.AddAsset();
		sphere.SetName("Inspector Sphere");
		Assets::Mesh::CreateSphere(&sphere);
		auto& unready = library.mImportedMeshes.AddAsset();
		unready.SetName("Inspector Pending");
		auto& material = library.mImportedMaterials.AddAsset();
		material.SetName("Inspector White");
		Assets::MaterialTextureSet textures;
		textures.mData.push_back({Fallbacks::FallbacksModule::Get().GetDefaultWhiteImage(), Assets::TextureUsageType::Albedo});
		material.GetTextures().push_back(textures);
		material.Create(defaultMaterial->GetShader());

		MeshPreviewCache previews;
		auto* meshPreview = previews.GetPreview(&cube);
		auto* materialPreview = previews.GetMaterialPreview(&material);
		Check(meshPreview && materialPreview, "Mesh and material thumbnails have GPU textures");
		Check(Readback(meshPreview->GetTextureView()->GetTexture(), "Build/DebugX64/inspector-mesh-preview.ppm") > 1000,
			"Large uploaded geometry is measured from its GPU buffer and visible in the thumbnail");
		Check(Readback(materialPreview->GetTextureView()->GetTexture(), "Build/DebugX64/inspector-material-preview.ppm", true) > 1000,
			"Material sphere contains shaded geometry");
		auto& black = library.mImportedMaterials.AddAsset();
		black.SetName("Inspector Black");
		textures.mData.front().pTexture = Fallbacks::FallbacksModule::Get().GetDefaultBlackImage();
		black.GetTextures().push_back(textures);
		black.Create(defaultMaterial->GetShader());
		auto* blackPreview = previews.GetMaterialPreview(&black);
		Check(blackPreview && Readback(blackPreview->GetTextureView()->GetTexture(), "Build/DebugX64/inspector-black-preview.ppm", true) < 1000,
			"Material thumbnails sample their own albedo map");

		auto& editorContext = UI::UIModule::Get().GetContext();
		Open(editorContext, "Mesh");
		UI::UIModule::Get().Render();
		Readback(Graphics::GraphicsModule::Get().GetSwapChain()->GetCurrentBackBufferRTV()->GetTexture(),
			"Build/DebugX64/inspector-asset-picker.ppm");
		Platform::Event dismiss;
		dismiss.mType = Platform::EventType::Key;
		dismiss.mKey = Platform::Input::KEYCODE_ESCAPE;
		dismiss.mPressed = true;
		editorContext.ProcessEvent(dismiss);

		const auto entity = Core::Scene::Get().CreateEntity("Asset property fixture").GetEntityID();
		registry.emplace<Components::MeshComponent>(entity, &cube, &material);
		EntityEditor inspector;
		EditorHistory history;
		EditorColorPickerWindow colors;
		UI::UIContext context;
		context.SetViewport(360, 600, 1.25f);
		context.SetDefaultFont(UI::UIModule::Get().GetContext().GetDefaultFont());
		context.OnPreviewEvent = [&](const Platform::Event& event) { return EditorAssetPicker::ProcessEvent(context, event); };
		bool refresh = false;
		auto build = [&]() {
			context.Clear();
			context.OnPreviewEvent = [&](const Platform::Event& event) { return EditorAssetPicker::ProcessEvent(context, event); };
			context.SetDefaultFont(UI::UIModule::Get().GetContext().GetDefaultFont());
			auto scroll = std::make_unique<UI::ScrollBox>();
			scroll->SetContent(inspector.Build(entity, [&](bool structural) { refresh = structural; }, history, colors));
			context.AddToViewport(std::move(scroll));
			context.Layout();
		};
		build();
		Check(Text(context, "Inspector Cube") && Text(context, "Inspector White"), "Both fields show their current assignment");
		Open(context, "Mesh");
		Check(!Text(context, "Inspector White") || !dynamic_cast<UI::Button*>(Text(context, "Inspector White")->GetParent()),
			"Mesh picker excludes material registry entries");
		auto* pending = Text(context, "Inspector Pending");
		Check(pending && !pending->GetParent()->IsEnabled(), "Unready registry meshes cannot be assigned");
		auto* search = dynamic_cast<UI::TextBox*>(context.Find(context.GetFocusedWidget()));
		Check(search != nullptr, "Opening the dropdown focuses its search field");
		search->SetText("SPHERE");
		search->OnChanged("SPHERE");
		Check(!Text(context, "Inspector Pending"), "Search filters names without case sensitivity");
		Action(context, "Inspector Sphere");
		Check(registry.get<Components::MeshComponent>(entity).GetMesh() == &sphere && refresh,
			"Choosing a mesh commits and requests renderer refresh");
		Check(registry.get<Components::MeshComponent>(entity).GetMaterial() == &material, "Choosing a mesh preserves the material field");
		Check(history.Undo() && registry.get<Components::MeshComponent>(entity).GetMesh() == &cube, "Undo restores the previous mesh");
		Check(history.Redo() && registry.get<Components::MeshComponent>(entity).GetMesh() == &sphere, "Redo restores the assigned mesh");
		history.Clear();
		refresh = false;
		build();
		Open(context, "Mesh");
		Action(context, "Inspector Sphere");
		Check(!history.CanUndo() && !refresh, "Choosing the existing assignment creates no history or renderer work");
		build();
		Open(context, "Mesh");
		Action(context, "Clear");
		Check(!registry.get<Components::MeshComponent>(entity).GetMesh(), "Clear removes the mesh reference");
		Check(history.Undo(), "Clearing an asset can be undone");
		build();
		Open(context, "Material");
		Action(context, defaultMaterial->GetName());
		Check(registry.get<Components::MeshComponent>(entity).GetMaterial() == defaultMaterial, "Material picker assigns a registry material");
		Check(history.Undo() && registry.get<Components::MeshComponent>(entity).GetMaterial() == &material,
			"Material assignment restores its previous reference on undo");
		build();
		Open(context, "Material");
		Action(context, "Clear");
		Check(!registry.get<Components::MeshComponent>(entity).GetMaterial(), "Clear removes the material reference");
		build();
		Check(Text(context, "None"), "An empty reference is shown as None");
		Open(context, "Material");
		Platform::Event key;
		key.mType = Platform::EventType::Key;
		key.mKey = Platform::Input::KEYCODE_ESCAPE;
		key.mPressed = true;
		context.ProcessEvent(key);
		Check(!Find<EditorAssetPicker>(context, [](const auto&) { return true; }), "Escape dismisses without changing the assignment");
		Open(context, "Material");
		context.SetFocus(0);
		EditorAssetPicker::Update(context);
		Check(!Find<EditorAssetPicker>(context, [](const auto&) { return true; }), "Moving keyboard focus away dismisses the chooser");
		Open(context, "Material");
		Platform::Event focus;
		focus.mType = Platform::EventType::FocusChanged;
		focus.mFocused = false;
		context.ProcessEvent(focus);
		Check(!Find<EditorAssetPicker>(context, [](const auto&) { return true; }), "Native focus loss dismisses the chooser");
		focus.mFocused = true;
		context.ProcessEvent(focus);
		Open(context, "Mesh");
		const auto before = EditorEntityState::Capture(entity);
		Platform::Event outside;
		outside.mType = Platform::EventType::MouseButton;
		outside.mMouseX = 359;
		outside.mMouseY = 599;
		outside.mButton = MOUSE_BUTTON_LEFT;
		outside.mPressed = true;
		context.ProcessEvent(outside);
		outside.mPressed = false;
		context.ProcessEvent(outside);
		Check(!Find<EditorAssetPicker>(context, [](const auto&) { return true; }) && EditorEntityState::Capture(entity) == before,
			"Outside click dismisses without editing scene data");
		Open(context, "Mesh");
		context.Clear();
		Check(!Find<EditorAssetPicker>(context, [](const auto&) { return true; }), "Inspector teardown removes its chooser");
		build();
		history.Clear();
		Open(context, "Mesh");
		registry.destroy(entity);
		const auto replacement = Core::Scene::Get().CreateEntity("Replacement asset target").GetEntityID();
		registry.emplace<Components::MeshComponent>(replacement, &cube, &material);
		Action(context, "Inspector Sphere");
		Check(registry.get<Components::MeshComponent>(replacement).GetMesh() == &cube && !history.CanUndo(),
			"A stale chooser cannot edit an entity reusing the deleted target's slot");
		context.Clear();
		registry.destroy(replacement);

		auto* workspace = editor.GetWorkspace();
		workspace->Detach("inspector");
		frame();
		Check(workspace->GetWindows().size() == 1, "Inspector can move into its own native window");
		auto& floating = *workspace->GetWindows().front();
		floating.GetWindow().Display(false);
		auto& detachedContext = floating.GetContext();
		Open(detachedContext, "Mesh");
		Action(detachedContext, "Inspector Cube");
		frame();
		Check(registry.get<Components::MeshComponent>(mainEntity).GetMesh() == &cube,
			"A detached inspector assigns through the actual editor controller");
		Open(detachedContext, "Material");
		Action(detachedContext, "Inspector White");
		frame();
		Check(registry.get<Components::MeshComponent>(mainEntity).GetMaterial() == &material &&
			registry.get<Components::MeshComponent>(mainEntity).GetRenderQueue() > 0,
			"A detached material assignment rebuilds the renderer and submits geometry");
		floating.Render();
		Readback(floating.GetSwapChain()->GetCurrentBackBufferRTV()->GetTexture(), "Build/DebugX64/inspector-assets-detached.ppm");
		workspace->Dock("inspector", EditorDockRegion::Right);
		frame();

		// Exercise the actual renderer after assigning one texture set to a higher mesh slot.
		sphere.mSubMeshes.front().data.TexSetIndex = 7;
		registry.get<Components::MeshComponent>(mainEntity).SetMesh(&sphere);
		registry.get<Components::MeshComponent>(mainEntity).SetMaterial(&material);
		frame();
		Check(registry.get<Components::MeshComponent>(mainEntity).GetRenderQueue() > 0,
			"An independently selected material safely renders unmatched mesh slots with its first texture set");
		registry.get<Components::MeshComponent>(mainEntity).SetMesh(nullptr);
		frame();
		Check(registry.get<Components::MeshComponent>(mainEntity).GetRenderQueue() == 0, "Cleared mesh stops drawing");
		previews.Clear();
		inspector.Reset();
		editor.Shutdown();
		std::cout << "Asset properties: thumbnails, typed registry selection, search, clear, history, dismissal, DPI and draw submission passed.\n";
	}
}
