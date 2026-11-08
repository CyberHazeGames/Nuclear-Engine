#include "WorkspaceTests.h"
#include <NuclearEditor.h>
#include <GameViewPort.h>
#include <AssetDocument.h>
#include <Nuclear.Platform/Platform.h>
#include <Diligent/Graphics/GraphicsEngine/interface/Texture.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cmath>

namespace Nuclear::Editor::Tests
{
	using namespace UI;
	namespace
	{
		void Check(bool condition, const char* message)
		{
			if (!condition) throw std::runtime_error(message);
		}
		UIContext& Main() { return UIModule::Get().GetContext(); }
		template<class T, class Predicate> T* Find(Predicate predicate, UIContext& context = Main())
		{
			Widget marker;
			for (WidgetId id = 1; id < marker.GetId(); ++id)
				if (auto* widget = dynamic_cast<T*>(context.Find(id)); widget && predicate(*widget)) return widget;
			return nullptr;
		}
		void Event(NuclearEditor& editor, UIContext& context, Platform::Window* window, Platform::Event event)
		{
			event.pWindow = window;
			if (window) editor.OnAuxiliaryWindowEvent(event);
			else context.ProcessEvent(event);
		}
		void Pointer(NuclearEditor& editor, UIContext& context, Platform::Window* window,
			Platform::EventType type, Vec2 point, bool down = false)
		{
			Platform::Event event;
			event.mType = type;
			event.mMouseX = int(std::lround(point.x * context.GetScale()));
			event.mMouseY = int(std::lround(point.y * context.GetScale()));
			event.mButton = MOUSE_BUTTON_LEFT;
			event.mPressed = down;
			Event(editor, context, window, event);
		}
		void Click(NuclearEditor& editor, Widget& widget, Platform::Window* window = nullptr)
		{
			auto& context = *widget.GetContext();
			context.Layout();
			for (auto* parent = widget.GetParent(); parent; parent = parent->GetParent())
				if (auto* scroll = dynamic_cast<ScrollBox*>(parent))
				{
					scroll->ScrollIntoView(widget);
					context.Layout();
				}
			const auto bounds = widget.GetBounds();
			const Vec2 point = {bounds.x + bounds.Width * .5f, bounds.y + bounds.Height * .5f};
			Pointer(editor, context, window, Platform::EventType::MouseButton, point, true);
			Pointer(editor, context, window, Platform::EventType::MouseButton, point);
		}
		void Action(NuclearEditor& editor, const std::string& text)
		{
			std::cout << "Workspace action: " << text << std::endl;
			auto* label = Find<TextBlock>([&](const auto& label) { return label.GetText() == text && label.IsVisible(); });
			Check(label && label->GetParent(), "Workspace action label exists");
			Click(editor, *label->GetParent());
		}
		void Tab(NuclearEditor& editor, EditorTabStrip& strip, const std::string& id)
		{
			Main().Layout();
			const auto bounds = strip.GetTabBounds(id);
			Check(bounds.Width > 30 && bounds.Height > 0, "Tab header is reachable");
			const Vec2 point = {bounds.x + 20, bounds.y + bounds.Height * .5f};
			Pointer(editor, Main(), nullptr, Platform::EventType::MouseButton, point, true);
			Pointer(editor, Main(), nullptr, Platform::EventType::MouseButton, point);
		}
		void Readback(const char* path, Diligent::ITexture* source)
		{
			using namespace Diligent;
			auto& graphics = Graphics::GraphicsModule::Get();
			auto* context = graphics.GetContext();
			auto desc = source->GetDesc();
			desc.Usage = USAGE_STAGING;
			desc.BindFlags = BIND_NONE;
			desc.CPUAccessFlags = CPU_ACCESS_READ;
			RefCntAutoPtr<ITexture> staging;
			graphics.GetDevice()->CreateTexture(desc, nullptr, &staging);
			Check(staging != nullptr, "Workspace readback allocation");
			context->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
			CopyTextureAttribs copy;
			copy.pSrcTexture = source;
			copy.pDstTexture = staging;
			copy.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
			copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
			context->CopyTexture(copy);
			context->WaitForIdle();
			MappedTextureSubresource mapped;
			context->MapTextureSubresource(staging, 0, 0, MAP_READ, MAP_FLAG_NONE, nullptr, mapped);
			Check(mapped.pData != nullptr, "Workspace readback mapping");
			std::ofstream output(path, std::ios::binary);
			output << "P6\n" << desc.Width << ' ' << desc.Height << "\n255\n";
			unsigned ink = 0;
			for (unsigned y = 0; y < desc.Height; ++y)
				for (unsigned x = 0; x < desc.Width; ++x)
				{
					auto* pixel = static_cast<const unsigned char*>(mapped.pData) + y * mapped.Stride + x * 4;
					output.write(reinterpret_cast<const char*>(pixel), 3);
					if (pixel[0] > 80 || pixel[1] > 100 || pixel[2] > 100) ++ink;
				}
			context->UnmapTextureSubresource(staging, 0, 0);
			Check(output.good() && ink > 600, "Workspace window renders visible text/content to its own target");
		}
	}

	void RunWorkspaceTests(NuclearEditor& editor, const std::function<void()>& frame)
	{
		auto& engine = Core::Engine::Get();
		engine.GetMainWindow()->SetSize(1440, 900);
		editor.OnWindowResize(1440, 900);
		frame();
		auto* workspace = editor.GetWorkspace();
		Check(workspace != nullptr, "Editor exposes a session workspace");
		auto* layout = workspace->GetDocumentLayout("viewport");
		auto* browser = Find<EditorTabStrip>([](const auto& strip) {
			return strip.IsVisible() && strip.GetTabBounds("viewport").Height > 0;
		});
		auto* viewport = Find<GameViewPort>([](const auto&) { return true; });
		Check(layout && browser && viewport, "Browser header and dock surface exist");
		auto& documents = *workspace->GetArea("viewport");
		auto* viewportHierarchy = workspace->GetArea("hierarchy");
		auto* viewportInspector = workspace->GetArea("inspector");
		Check(documents.GetTabs().size() == 1 && documents.GetActive() == "viewport", "Main viewport starts as a document tab");
		auto checkSceneToolbar = [](bool visible) {
			for (const char* title : {"New project", "New scene", "Add", "VSync"})
			{
				auto* label = Find<TextBlock>([title](const auto& label) { return label.GetText() == title; });
				Check(label && label->IsVisible() == visible, "Scene creation toolbar appears only on Main viewport");
			}
		};
		checkSceneToolbar(true);
		const float viewportWorkspaceTop = layout->GetBounds().y;
		Action(editor, "Open / editorfixtures_triangle");
		frame();
		const auto meshId = documents.GetActive();
		Check(meshId != "viewport" && documents.GetTabs().size() == 2 && !viewport->IsVisible(),
			"Opening a mesh creates an active details tab and hides the main viewport");
		auto* meshLayout = workspace->GetDocumentLayout(meshId);
		checkSceneToolbar(false);
		Check(meshLayout && meshLayout->GetBounds().y < viewportWorkspaceTop - 20,
			"Asset documents reclaim the space occupied by the scene creation toolbar");
		Check(meshLayout && meshLayout != layout && !viewportHierarchy->IsVisible() && !viewportInspector->IsVisible(),
			"An asset document owns a separate dock layout with no viewport hierarchy or inspector");
		Check(meshLayout->GetArea(EditorDockRegion::Documents).GetBounds().Width > layout->GetBounds().Width - 12 &&
			meshLayout->GetArea(EditorDockRegion::Right).GetTabs().empty(),
			"An asset viewer starts with full-width content and no inherited detail panels");
		Check(Find<TextBlock>([](const auto& label) { return label.GetText() == "Vertices / 3"; }) &&
			Find<TextBlock>([](const auto& label) { return label.GetText() == "Triangles / 1"; }),
			"Created mesh documents retain correct counts after decoded arrays are released");
		UIModule::Get().Render();
		Readback("Build/DebugX64/editor-document.ppm", Graphics::GraphicsModule::Get().GetSwapChain()->GetCurrentBackBufferRTV()->GetTexture());
		Tab(editor, *browser, "viewport");
		frame();
		checkSceneToolbar(true);
		Action(editor, "Open / editorfixtures_triangle");
		frame();
		Check(documents.GetTabs().size() == 2, "Reopening an asset activates its existing tab");
		Tab(editor, *browser, "viewport");
		frame();
		const auto fixture = std::filesystem::absolute("Build/DebugX64/editorfixtures/preview.bmp");
		std::array<unsigned char, 70> bitmap{};
		bitmap[0] = 'B';
		bitmap[1] = 'M';
		bitmap[2] = 70;
		bitmap[10] = 54;
		bitmap[14] = 40;
		bitmap[18] = 2;
		bitmap[22] = 2;
		bitmap[26] = 1;
		bitmap[28] = 24;
		bitmap[34] = 16;
		bitmap[54] = bitmap[58] = bitmap[64] = 255;
		bitmap[65] = bitmap[66] = bitmap[67] = 255;
		{
			std::ofstream image(fixture, std::ios::binary);
			image.write(reinterpret_cast<const char*>(bitmap.data()), bitmap.size());
		}
		Action(editor, "Files");
		frame();
		Action(editor, "Import / preview.bmp");
		frame();
		Action(editor, "Textures");
		frame();
		Action(editor, "Open / editorfixtures_preview");
		frame();
		const auto textureId = documents.GetActive();
		checkSceneToolbar(false);
		auto* textureHeading = Find<TextBlock>([](const auto& label) { return label.GetText() == "TEXTURE DETAILS"; });
		Check(documents.GetTabs().size() == 3 && textureHeading, "Texture details open as another browser document");
		Assets::Texture* previewTexture = nullptr;
		for (auto& [id, asset] : Assets::AssetLibrary::Get().mImportedTextures.mData)
			if (asset.GetName() == "editorfixtures_preview")
				previewTexture = &asset;
		Check(previewTexture && previewTexture->GetState() == Assets::IAsset::State::Created && previewTexture->GetTextureView(),
			"Synchronous texture import preserves the uploaded resource's Created state");
		bool sampledTexture = false;
		for (const auto& quad : Main().BuildDrawList().Quads)
			if (quad.pTexture == previewTexture->GetTextureView() && quad.Bounds.Width > 200)
				sampledTexture = true;
		Check(sampledTexture, "Texture document samples the real GPU image instead of a solid fallback");
		const auto headingId = textureHeading->GetId();
		UIModule::Get().Render();
		Readback("Build/DebugX64/editor-texture-document.ppm", Graphics::GraphicsModule::Get().GetSwapChain()->GetCurrentBackBufferRTV()->GetTexture());
		const auto closeTab = browser->GetTabBounds(textureId);
		const Vec2 closePoint{closeTab.x + closeTab.Width - 12, closeTab.y + 16};
		Pointer(editor, Main(), nullptr, Platform::EventType::MouseButton, closePoint, true);
		Pointer(editor, Main(), nullptr, Platform::EventType::MouseButton, closePoint);
		frame();
		Check(documents.GetTabs().size() == 2 && !Main().Find(headingId), "Tab close button removes the document from the active tree");
		Tab(editor, *browser, "viewport");
		frame();
		Action(editor, "Open / editorfixtures_preview");
		frame();
		Check(Main().Find(headingId) == textureHeading, "Reopening a closed document restores its original contents");
		Tab(editor, *browser, "viewport");
		frame();
		Action(editor, "Materials");
		frame();
		Action(editor, "Open / Editor / Default material");
		frame();
		Check(documents.GetTabs().size() == 4, "Material details open beside texture, mesh and viewport tabs");
		const auto materialId = documents.GetActive();
		checkSceneToolbar(false);
		auto titleOf = [&documents](const std::string& id) {
			for (const auto& tab : documents.GetTabs())
				if (tab.Id == id) return tab.Title;
			return std::string{};
		};
		Check(AssetDocument::GetTitle(Assets::AssetType::Texture, " \t\r\n") == "Unnamed Texture" &&
			AssetDocument::GetTitle(Assets::AssetType::Mesh, "") == "Unnamed Mesh" &&
			AssetDocument::GetTitle(Assets::AssetType::Material, "") == "Unnamed Material" &&
			AssetDocument::GetTitle(Assets::AssetType::Shader, "") == "Unnamed Shader" &&
			AssetDocument::GetTitle(Assets::AssetType::Texture, "  Named texture  ") == "Named texture",
			"Asset titles use their type for blank names and retain meaningful authored names");
		// Several built-in resources can be unnamed too; keep each UI action unambiguous for this fixture.
		auto distinguishOtherUnnamed = [](auto& map, Assets::IAsset* fixture) {
			std::vector<std::pair<Assets::IAsset*, std::string>> originals;
			for (auto& [id, asset] : map.mData)
			{
				if (&asset != fixture && asset.GetName().find_first_not_of(" \t\r\n") == std::string::npos)
				{
					originals.emplace_back(&asset, asset.GetName());
					asset.SetName("Title fixture / " + id.str());
				}
			}
			return originals;
		};
		auto otherTextures = distinguishOtherUnnamed(Assets::AssetLibrary::Get().mImportedTextures, previewTexture);
		previewTexture->SetName("");
		Tab(editor, *browser, "viewport");
		frame();
		Action(editor, "Textures");
		frame();
		Action(editor, "Open / Unnamed Texture");
		frame();
		Check(documents.GetActive() == textureId && titleOf(textureId) == "Unnamed Texture",
			"Reopening an unnamed texture keeps its identity and supplies a nonempty browser title");
		for (auto& [asset, original] : otherTextures)
			asset->SetName(original);
		previewTexture->SetName("editorfixtures_preview");
		workspace->SetTitle(textureId, previewTexture->GetName());
		Assets::Mesh* meshAsset = nullptr;
		for (auto& [id, asset] : Assets::AssetLibrary::Get().mImportedMeshes.mData)
			if (asset.GetName() == "editorfixtures_triangle") meshAsset = &asset;
		Check(meshAsset != nullptr, "Imported mesh exists for title checks");
		auto otherMeshes = distinguishOtherUnnamed(Assets::AssetLibrary::Get().mImportedMeshes, meshAsset);
		meshAsset->SetName(" \t");
		Tab(editor, *browser, "viewport");
		frame();
		Action(editor, "Meshes");
		frame();
		Action(editor, "Open / Unnamed Mesh");
		frame();
		Check(titleOf(meshId) == "Unnamed Mesh", "Whitespace-only mesh names receive a type-specific title");
		for (auto& [asset, original] : otherMeshes)
			asset->SetName(original);
		meshAsset->SetName("editorfixtures_triangle");
		workspace->SetTitle(meshId, meshAsset->GetName());
		Assets::Material* materialAsset = nullptr;
		for (auto& [id, asset] : Assets::AssetLibrary::Get().mImportedMaterials.mData)
			if (asset.GetName() == "Editor / Default material") materialAsset = &asset;
		Check(materialAsset != nullptr, "Editor material exists for title checks");
		auto otherMaterials = distinguishOtherUnnamed(Assets::AssetLibrary::Get().mImportedMaterials, materialAsset);
		materialAsset->SetName("");
		Tab(editor, *browser, "viewport");
		frame();
		Action(editor, "Materials");
		frame();
		Action(editor, "Open / Unnamed Material");
		frame();
		Check(titleOf(materialId) == "Unnamed Material" && documents.GetTabs().size() == 4,
			"Unnamed material titles stay nonempty without creating duplicate documents");
		for (auto& [asset, original] : otherMaterials)
			asset->SetName(original);
		materialAsset->SetName("Editor / Default material");
		workspace->SetTitle(materialId, materialAsset->GetName());

		// Future custom asset panels attach to that asset's layout, never the viewport or another asset.
		auto customPanel = std::make_unique<TextBox>("private mesh editor state");
		auto* meshPanel = customPanel.get();
		workspace->Add("mesh-private", "Mesh custom panel", std::move(customPanel),
			EditorDockRegion::Right, false, true, true, meshId);
		Check(!meshPanel->IsVisible(), "A custom mesh panel stays hidden while a different asset is active");
		Tab(editor, *browser, meshId);
		frame();
		Check(meshPanel->IsVisible() && !viewportInspector->IsVisible() && !viewportHierarchy->IsVisible(),
			"An asset displays only its own registered panels");
		workspace->Dock("mesh-private", EditorDockRegion::Left);
		frame();
		Check(workspace->GetArea("mesh-private") == &meshLayout->GetArea(EditorDockRegion::Left),
			"A custom asset panel docks inside its owning document");
		workspace->ResetLayout();
		frame();
		Check(documents.GetActive() == meshId && meshPanel->IsVisible() && !viewportInspector->IsVisible() &&
			workspace->GetArea("mesh-private") == &meshLayout->GetArea(EditorDockRegion::Right),
			"Reset layout restores only the active asset's panels and keeps that asset selected");
		Action(editor, "Window");
		Action(editor, "Manage panels...");
		Check(!Find<TextBlock>([](const auto& label) { return label.IsVisible() && label.GetText() == "Show Inspector"; }) &&
			Find<TextBlock>([](const auto& label) { return label.IsVisible() && label.GetText() == "Show Mesh custom panel"; }),
			"The Window menu lists only the active document's custom tools");
		Action(editor, "Close menu");
		workspace->Detach("inspector");
		frame();
		Check(workspace->GetWindows().size() == 1, "A viewport panel can remain in its own native window");
		auto& foreignWindow = *workspace->GetWindows().front();
		foreignWindow.GetWindow().Display(false);
		Platform::Event foreignFocus;
		foreignFocus.mType = Platform::EventType::FocusChanged;
		foreignFocus.mFocused = true;
		Event(editor, foreignWindow.GetContext(), &foreignWindow.GetWindow(), foreignFocus);
		const auto foreignTab = foreignWindow.GetArea().GetStrip().GetTabBounds("inspector");
		const Vec2 foreignStart{foreignTab.x + 20, foreignTab.y + 16};
		const auto assetBounds = meshLayout->GetBounds();
		int foreignX = int((assetBounds.x + assetBounds.Width * .5f) * Main().GetScale());
		int foreignY = int((assetBounds.y + 40) * Main().GetScale());
		Check(engine.GetMainWindow()->ClientToScreen(foreignX, foreignY) &&
			foreignWindow.GetWindow().ScreenToClient(foreignX, foreignY), "Foreign panel drag coordinates convert");
		const float foreignScale = foreignWindow.GetContext().GetScale();
		const Vec2 foreignDrop{foreignX / foreignScale, foreignY / foreignScale};
		Pointer(editor, foreignWindow.GetContext(), &foreignWindow.GetWindow(), Platform::EventType::MouseButton, foreignStart, true);
		Pointer(editor, foreignWindow.GetContext(), &foreignWindow.GetWindow(), Platform::EventType::MouseMove, foreignDrop);
		Pointer(editor, foreignWindow.GetContext(), &foreignWindow.GetWindow(), Platform::EventType::MouseButton, foreignDrop);
		frame();
		Check(workspace->GetWindows().size() == 1 && workspace->GetArea("inspector") == &foreignWindow.GetArea() &&
			documents.GetActive() == meshId, "A viewport inspector cannot dock into an asset document");
		Platform::Event foreignClose;
		foreignClose.mType = Platform::EventType::WindowClose;
		Event(editor, foreignWindow.GetContext(), &foreignWindow.GetWindow(), foreignClose);
		frame();
		Check(workspace->GetWindows().empty() && documents.GetActive() == meshId &&
			!workspace->GetArea("inspector")->IsVisible(), "Closing a foreign panel redocks it into its inactive owner only");
		const auto customPanelId = meshPanel->GetId();
		workspace->Close(meshId);
		frame();
		Check(!Main().Find(customPanelId), "Closing an asset parks its complete custom panel tree");
		workspace->Show(meshId);
		frame();
		Check(Main().Find(customPanelId) == meshPanel && meshPanel->IsVisible() &&
			meshPanel->GetText() == "private mesh editor state", "Reopening an asset restores its own custom panels and state");
		Tab(editor, *browser, textureId);
		frame();
		Check(!meshPanel->IsVisible() && workspace->GetDocumentLayout(textureId)->GetArea(EditorDockRegion::Right).GetTabs().empty(),
			"Asset-specific panels do not leak into another asset viewer");
		Tab(editor, *browser, "viewport");
		frame();
		Check(viewport->IsVisible() && viewportInspector->IsVisible() && viewportHierarchy->IsVisible() &&
			!meshPanel->IsVisible() && meshPanel->GetText() == "private mesh editor state",
			"Switching back restores viewport tools while preserving parked asset-panel state");
		checkSceneToolbar(true);
		workspace->Close("viewport");
		frame();
		Check(workspace->GetArea("viewport") == &documents, "Pinned main viewport cannot be closed");

		// Reorder through real retained pointer capture, rather than a test-only controller command.
		const auto source = browser->GetTabBounds(meshId);
		const auto target = browser->GetTabBounds("viewport");
		const Vec2 dragStart{source.x + 20, source.y + 16};
		const Vec2 dragEnd{target.x + 3, target.y + 16};
		Pointer(editor, Main(), nullptr, Platform::EventType::MouseButton, dragStart, true);
		Pointer(editor, Main(), nullptr, Platform::EventType::MouseMove, dragEnd);
		Pointer(editor, Main(), nullptr, Platform::EventType::MouseButton, dragEnd);
		frame();
		Check(documents.GetTabs().front().Id == meshId, "Browser document tabs can be reordered by dragging");
		Tab(editor, *browser, "viewport");
		frame();

		auto* hierarchyArea = workspace->GetArea("hierarchy");
		auto* inspectorArea = workspace->GetArea("inspector");
		auto* filter = Find<TextBox>([&](const auto& input) {
			for (auto* parent = input.GetParent(); parent; parent = parent->GetParent())
				if (parent == hierarchyArea) return true;
			return false;
		});
		Check(filter != nullptr, "Hierarchy filter exists before docking");
		filter->SetText("preserved filter text");
		const auto filterId = filter->GetId();
		const auto tab = hierarchyArea->GetStrip().GetTabBounds("hierarchy");
		const auto dockBounds = inspectorArea->GetBounds();
		const Vec2 start{tab.x + 20, tab.y + 16};
		const Vec2 end{dockBounds.x + 35, dockBounds.y + 17};
		Pointer(editor, Main(), nullptr, Platform::EventType::MouseButton, start, true);
		Pointer(editor, Main(), nullptr, Platform::EventType::MouseMove, end);
		Pointer(editor, Main(), nullptr, Platform::EventType::MouseButton, end);
		frame();
		Check(workspace->GetArea("hierarchy") == workspace->GetArea("inspector") &&
			Main().Find(filterId) == filter && filter->GetText() == "preserved filter text",
			"Docking groups tabs without rebuilding or losing control state");
		Tab(editor, *browser, meshId);
		frame();
		Check(!workspace->GetArea("hierarchy")->IsVisible() && meshPanel->IsVisible(),
			"Grouped viewport panels remain isolated when an asset document is selected");
		Tab(editor, *browser, "viewport");
		frame();
		Check(workspace->GetArea("hierarchy") == workspace->GetArea("inspector") &&
			filter->GetText() == "preserved filter text", "Returning to the viewport restores its prior dock arrangement");
		workspace->ResetLayout();
		frame();
		filter->SetText("");

		// A tab tear-off creates an actual native window, private input context and GPU target.
		hierarchyArea = workspace->GetArea("hierarchy");
		const auto floatTab = hierarchyArea->GetStrip().GetTabBounds("hierarchy");
		const Vec2 floatStart{floatTab.x + 20, floatTab.y + 16};
		Pointer(editor, Main(), nullptr, Platform::EventType::MouseButton, floatStart, true);
		Pointer(editor, Main(), nullptr, Platform::EventType::MouseMove, {-100, -100});
		Pointer(editor, Main(), nullptr, Platform::EventType::MouseButton, {-100, -100});
		frame();
		Check(workspace->GetWindows().size() == 1, "Dragging a panel out creates a native window");
		auto& floating = *workspace->GetWindows().front();
		floating.GetWindow().Display(false);
		workspace->SetTitle("hierarchy", "Renamed hierarchy");
		Check(Find<TextBlock>([](const auto& label) { return label.GetText() == "Nuclear / Renamed hierarchy"; }, floating.GetContext()),
			"Renaming a detached panel updates its retained title bar");
		workspace->SetTitle("hierarchy", "Hierarchy");
		Check(floating.GetWindow().GetNativeHandle() && floating.GetContext().Find(filterId) == filter &&
			!Main().Find(filterId), "The original panel tree moves into an independent context");
		floating.Render();
		Readback("Build/DebugX64/editor-floating.ppm", floating.GetSwapChain()->GetCurrentBackBufferRTV()->GetTexture());
		floating.Present();
		Platform::Event focus;
		focus.mType = Platform::EventType::FocusChanged;
		focus.mFocused = true;
		Event(editor, floating.GetContext(), &floating.GetWindow(), focus);
		Click(editor, *filter, &floating.GetWindow());
		Platform::Event text;
		text.mType = Platform::EventType::TextInput;
		text.mCodepoint = 'Z';
		Event(editor, floating.GetContext(), &floating.GetWindow(), text);
		Check(filter->GetText() == "Z", "Detached panel accepts independent text input");
		entt::entity historyEntity = entt::null;
		for (auto entity : Core::Scene::Get().GetRegistry().view<Components::EntityInfoComponent>())
			if (Core::Scene::Get().GetRegistry().get<Components::EntityInfoComponent>(entity).mName == "editorfixtures_triangle")
				historyEntity = entity;
		Check(historyEntity != entt::null, "Detached shortcut target is the instantiated library mesh");
		auto historyKey = [&](Platform::Input::KeyCode key) {
			Platform::Event input;
			input.mType = Platform::EventType::Key;
			input.mKey = Platform::Input::KEYCODE_LCTRL;
			input.mPressed = true;
			Event(editor, floating.GetContext(), &floating.GetWindow(), input);
			input.mKey = key;
			Event(editor, floating.GetContext(), &floating.GetWindow(), input);
			input.mPressed = false;
			Event(editor, floating.GetContext(), &floating.GetWindow(), input);
			input.mKey = Platform::Input::KEYCODE_LCTRL;
			Event(editor, floating.GetContext(), &floating.GetWindow(), input);
		};
		Click(editor, *filter, &floating.GetWindow());
		historyKey(Platform::Input::KEYCODE_Z);
		Check(Core::Scene::Get().GetRegistry().valid(historyEntity), "Detached text editing isolates scene shortcuts");
		floating.GetContext().SetFocus(0);
		historyKey(Platform::Input::KEYCODE_Z);
		frame();
		Check(!Core::Scene::Get().GetRegistry().valid(historyEntity), "Ctrl+Z from a detached workspace undoes the scene action");
		historyKey(Platform::Input::KEYCODE_Y);
		frame();
		Check(Core::Scene::Get().GetRegistry().valid(historyEntity), "Ctrl+Y from a detached workspace redoes the scene action");
		floating.GetWindow().SetSize(680, 500);
		Platform::Event queued;
		while (Platform::PollEvent(queued))
			if (queued.pWindow == &floating.GetWindow()) editor.OnAuxiliaryWindowEvent(queued);
		frame();
		Check(floating.GetContext().GetPixelSize().x == 680 && floating.GetContext().GetPixelSize().y == 500,
			"Native secondary-window resize reaches only its retained context");
		Check(std::abs(floating.GetContext().GetScale() - floating.GetWindow().GetDpiScale()) < .001f,
			"Detached window resize uses its own monitor DPI");
		// Exercise a noninteger secondary scale even on a single 96-DPI monitor.
		floating.GetContext().SetViewport(680, 500, 1.25f);
		floating.GetContext().Layout();
		Event(editor, floating.GetContext(), &floating.GetWindow(), focus);
		const auto floatingTab = floating.GetArea().GetStrip().GetTabBounds("hierarchy");
		const Vec2 floatingStart{floatingTab.x + 20, floatingTab.y + 16};
		const auto surfaceBounds = layout->GetBounds();
		int desktopX = int((surfaceBounds.x + 30) * Main().GetScale());
		int desktopY = int((surfaceBounds.y + 20) * Main().GetScale());
		Check(engine.GetMainWindow()->ClientToScreen(desktopX, desktopY) &&
			floating.GetWindow().ScreenToClient(desktopX, desktopY), "Cross-window drag coordinates convert through the desktop");
		const float floatingScale = floating.GetContext().GetScale();
		const Vec2 dockPoint{desktopX / floatingScale, desktopY / floatingScale};
		Pointer(editor, floating.GetContext(), &floating.GetWindow(), Platform::EventType::MouseButton, floatingStart, true);
		Pointer(editor, floating.GetContext(), &floating.GetWindow(), Platform::EventType::MouseMove, dockPoint);
		Pointer(editor, floating.GetContext(), &floating.GetWindow(), Platform::EventType::MouseButton, dockPoint);
		frame();
		Check(workspace->GetWindows().empty() && Main().Find(filterId) == filter,
			"A captured tab drag redocks from native-window coordinates into the main layout");
		workspace->Detach("hierarchy");
		frame();
		auto& closing = *workspace->GetWindows().front();
		closing.GetWindow().Display(false);
		Platform::Event close;
		close.mType = Platform::EventType::WindowClose;
		Event(editor, closing.GetContext(), &closing.GetWindow(), close);
		frame();
		Check(workspace->GetWindows().empty() && Main().Find(filterId) == filter && filter->GetText() == "Z",
			"Closing a native window redocks its unchanged contents without closing the editor");
		filter->SetText("");

		workspace->Close("inspector");
		frame();
		Check(!workspace->GetArea("inspector"), "Docked tool panels can be closed");
		Action(editor, "Window");
		Action(editor, "Manage panels...");
		Action(editor, "Show Inspector");
		frame();
		Check(workspace->GetArea("inspector") != nullptr, "Window menu restores closed tools");
		workspace->Detach(meshId);
		frame();
		Check(workspace->GetWindows().size() == 1, "Asset documents can also use native windows");
		checkSceneToolbar(true);
		workspace->GetWindows().front()->GetWindow().Display(false);
		Check(!Find<TextBlock>([](const auto& label) {
			return label.GetText() == "New project" || label.GetText() == "New scene" || label.GetText() == "Add";
		}, workspace->GetWindows().front()->GetContext()), "Detached asset documents do not inherit scene creation actions");
		Check(meshPanel->GetContext() == &workspace->GetWindows().front()->GetContext() &&
			!workspace->GetWindows().front()->GetContext().Find(viewportInspector->GetId()),
			"Detaching an asset moves its complete custom workspace without viewport panels");
		workspace->ResetLayout();
		frame();
		Check(workspace->GetWindows().size() == 1 && documents.GetActive() == "viewport",
			"Resetting the viewport layout leaves another document's native workspace intact");
		workspace->Dock(meshId, EditorDockRegion::Documents);
		workspace->Show("viewport");
		frame();
		Check(workspace->GetWindows().empty() && !meshPanel->IsVisible(),
			"Redocking a complete asset workspace preserves and isolates its custom panels");
		const float originalWidth = viewport->GetBounds().Width;
		const auto rightBounds = layout->GetArea(EditorDockRegion::Right).GetBounds();
		const Vec2 divider{rightBounds.x - 3, rightBounds.y + 60};
		Pointer(editor, Main(), nullptr, Platform::EventType::MouseButton, divider, true);
		Pointer(editor, Main(), nullptr, Platform::EventType::MouseMove, {divider.x - 50, divider.y});
		Pointer(editor, Main(), nullptr, Platform::EventType::MouseButton, {divider.x - 50, divider.y});
		frame();
		Check(viewport->GetBounds().Width < originalWidth - 30, "Dragging the dock splitter resizes adjacent panels");
		workspace->ResetLayout();
		frame();
		workspace->Detach("inspector");
		frame();
		Check(workspace->GetWindows().size() == 1, "Leave a native panel alive for teardown validation");
		workspace->GetWindows().front()->GetWindow().Display(false);
		auto& modalWindow = *workspace->GetWindows().front();
		auto* name = Find<TextBox>([](const auto& input) { return input.GetText() == "editorfixtures_triangle"; }, modalWindow.GetContext());
		Check(name != nullptr, "Detached inspector has the selected entity name");
		Action(editor, "New project");
		frame();
		Event(editor, modalWindow.GetContext(), &modalWindow.GetWindow(), focus);
		Click(editor, *name, &modalWindow.GetWindow());
		text.mCodepoint = 'X';
		Event(editor, modalWindow.GetContext(), &modalWindow.GetWindow(), text);
		Check(name->GetText() == "editorfixtures_triangle", "Project modal isolates input in independent windows");
		Action(editor, "Cancel");
		frame();
		std::cout << "Workspace document tabs, docking, tear-off, native input/render/resize, redock and recovery passed.\n";
	}
}
