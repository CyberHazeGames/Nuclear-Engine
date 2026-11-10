#include "AssetDragTests.h"
#include <NuclearEditor.h>
#include <EditorAssetButton.h>
#include <GameViewPort.h>
#include <MeshPlacementPreview.h>
#include <UI/TextBlock.h>
#include <UI/ScrollBox.h>
#include <Nuclear.Platform/Input.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <cstring>

namespace Nuclear::Editor::Tests
{
	namespace
	{
		void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
		std::vector<unsigned char> ReadColor(Components::CameraComponent& camera)
		{
			using namespace Diligent;
			auto* source = camera.GetColorRT().GetRTV()->GetTexture();
			auto desc = source->GetDesc();
			desc.Usage = USAGE_STAGING;
			desc.BindFlags = BIND_NONE;
			desc.CPUAccessFlags = CPU_ACCESS_READ;
			RefCntAutoPtr<ITexture> staging;
			auto& graphics = Graphics::GraphicsModule::Get();
			graphics.GetDevice()->CreateTexture(desc, nullptr, &staging);
			Check(staging != nullptr, "Placement color readback allocation");
			auto* gpu = graphics.GetContext();
			gpu->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
			CopyTextureAttribs copy;
			copy.pSrcTexture = source;
			copy.pDstTexture = staging;
			copy.SrcTextureTransitionMode = copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
			gpu->CopyTexture(copy);
			gpu->WaitForIdle();
			MappedTextureSubresource mapped;
			gpu->MapTextureSubresource(staging, 0, 0, MAP_READ, MAP_FLAG_NONE, nullptr, mapped);
			Check(mapped.pData != nullptr, "Placement color readback maps");
			std::vector<unsigned char> result(desc.Width * desc.Height * 4);
			for (Uint32 y = 0; y < desc.Height; ++y)
				std::memcpy(result.data() + y * desc.Width * 4, static_cast<const char*>(mapped.pData) + y * mapped.Stride, desc.Width * 4);
			gpu->UnmapTextureSubresource(staging, 0, 0);
			return result;
		}
		template<class T, class Predicate> T* Find(UI::UIContext& context, Predicate predicate)
		{
			UI::Widget marker;
			for (UI::WidgetId id = 1; id < marker.GetId(); ++id)
				if (auto* item = dynamic_cast<T*>(context.Find(id)); item && predicate(*item)) return item;
			return nullptr;
		}
		void Pointer(UI::UIContext& context, Platform::EventType type, UI::Vec2 point, bool down = false)
		{
			Platform::Event event;
			event.mType = type;
			event.mMouseX = int(std::lround(point.x * context.GetScale()));
			event.mMouseY = int(std::lround(point.y * context.GetScale()));
			event.mButton = MOUSE_BUTTON_LEFT;
			event.mPressed = down;
			context.ProcessEvent(event);
		}
		void Key(UI::UIContext& context, Platform::Input::KeyCode key, bool down = true)
		{
			Platform::Event event;
			event.mType = Platform::EventType::Key;
			event.mKey = key;
			event.mPressed = down;
			context.ProcessEvent(event);
		}
	}

	void RunAssetDragTests()
	{
		auto& engine = Core::Engine::Get();
		NuclearEditor editor;
		editor.Load();
		auto& context = UI::UIModule::Get().GetContext();
		auto frame = [&]() {
			engine.BeginFrame();
			editor.Update(1.f / 60);
			editor.Render(1.f / 60);
			engine.EndFrame();
		};
		auto& mesh = Assets::AssetLibrary::Get().mImportedMeshes.AddAsset();
		mesh.SetName("Placement cube");
		Assets::Mesh::CreateCube(&mesh);
		mesh.SetState(Assets::IAsset::State::Created);
		frame();
		auto* meshes = Find<UI::TextBlock>(context, [](const auto& text) { return text.IsVisible() && text.GetText() == "Meshes"; });
		Check(meshes != nullptr, "Mesh category exists");
		dynamic_cast<UI::Button*>(meshes->GetParent())->OnClicked();
		frame();
		auto sourceIn = [](UI::UIContext& owner) {
			auto* label = Find<UI::TextBlock>(owner, [](const auto& text) {
				if (!text.IsVisible() || text.GetText() != "Placement cube") return false;
				for (auto* parent = text.GetParent(); parent; parent = parent->GetParent())
					if (dynamic_cast<EditorAssetButton*>(parent)) return true;
				return false;
			});
			UI::Widget* item = label;
			while (item && !dynamic_cast<EditorAssetButton*>(item)) item = item->GetParent();
			Check(item != nullptr, "Mesh thumbnail has a captured drag button");
			for (auto* parent = item->GetParent(); parent; parent = parent->GetParent())
				if (auto* scroll = dynamic_cast<UI::ScrollBox*>(parent)) scroll->ScrollIntoView(*item);
			owner.Layout();
			return static_cast<EditorAssetButton*>(item);
		};
		auto* source = sourceIn(context);
		auto* viewport = Find<GameViewPort>(context, [](const auto& widget) { return widget.IsVisible(); });
		Check(viewport != nullptr, "Scene drop target is visible");
		auto center = [](const UI::Widget& item) {
			const auto bounds = item.GetBounds();
			return UI::Vec2{bounds.x + bounds.Width / 2, bounds.y + bounds.Height / 2};
		};
		auto instances = [&]() {
			std::vector<entt::entity> found;
			auto& registry = Core::Scene::Get().GetRegistry();
			for (auto entity : registry.view<Components::MeshComponent>())
				if (registry.get<Components::MeshComponent>(entity).GetMesh() == &mesh) found.push_back(entity);
			return found;
		};
		const auto target = center(*viewport);
		const auto start = center(*source);
		auto& camera = *Core::Scene::Get().GetMainCamera();
		const auto beforePreview = ReadColor(camera);
		Pointer(context, Platform::EventType::MouseButton, start, true);
		Pointer(context, Platform::EventType::MouseMove, target);
		frame();
		Check(instances().empty(), "Hover preview creates no authored scene entity");
		const auto previewPixels = ReadColor(camera);
		size_t changed = 0;
		for (size_t i = 0; i + 3 < previewPixels.size() && i + 3 < beforePreview.size(); i += 4)
			if (previewPixels[i] != beforePreview[i] || previewPixels[i + 1] != beforePreview[i + 1] || previewPixels[i + 2] != beforePreview[i + 2]) ++changed;
		Check(changed > 100, "Dragging paints a visible mesh ghost in the scene target");
		const auto dimensions = camera.GetColorRT().GetDimensions();
		std::ofstream image("Build/DebugX64/asset-drag-preview.ppm", std::ios::binary);
		image << "P6\n" << dimensions.x << ' ' << dimensions.y << "\n255\n";
		for (size_t i = 0; i + 3 < previewPixels.size(); i += 4) image.write(reinterpret_cast<const char*>(previewPixels.data() + i), 3);
		image.close();
		Check(context.GetCapturedWidget() == source->GetId(), "Mesh drag retains source pointer capture");
		MeshPlacementPreview preview;
		preview.Begin(mesh.GetUUID());
		Check(preview.Update(camera, viewport->GetBounds(), target), "Scene ray resolves a placement point");
		const auto contact = preview.GetPosition();
		Check(contact.y > .55f || std::abs(contact.z) > .9f, "Placement uses rendered mesh surfaces rather than only the ground plane");
		preview.Render(camera);
		Check(preview.Update(camera, viewport->GetBounds(), target) && Math::length(preview.GetPosition() - contact) < .001f,
			"Ghost rendering preserves scene depth and does not move the next contact point");
		const auto sceneBounds = viewport->GetBounds();
		Check(preview.Update(camera, sceneBounds, {sceneBounds.x + 10, sceneBounds.y + sceneBounds.Height - 10}) &&
			std::abs(preview.GetPosition().y - .5f) < .01f, "Empty scene background falls back to ground contact");
		Check(!preview.Update(camera, sceneBounds, {-10, -10}), "Placement rejects points outside the viewport");
		mesh.SetState(Assets::IAsset::State::Loaded);
		Check(!preview.Update(camera, sceneBounds, target), "Placement requires created mesh resources");
		mesh.SetState(Assets::IAsset::State::Created);
		Pointer(context, Platform::EventType::MouseButton, target);
		frame();
		auto placed = instances();
		Check(placed.size() == 1, "Release in the viewport places exactly one mesh");
		const auto position = Core::Scene::Get().GetRegistry().get<Components::EntityInfoComponent>(placed[0]).mTransform.GetLocalPosition();
		Check(Math::length(position - contact) < .05f, "Mesh bounds rest at the previewed scene/ground contact");
		Key(context, Platform::Input::KEYCODE_LCTRL);
		Key(context, Platform::Input::KEYCODE_Z);
		Key(context, Platform::Input::KEYCODE_Z, false);
		Key(context, Platform::Input::KEYCODE_LCTRL, false);
		frame();
		Check(instances().empty(), "One undo removes the completed placement");
		Key(context, Platform::Input::KEYCODE_LCTRL);
		Key(context, Platform::Input::KEYCODE_Y);
		Key(context, Platform::Input::KEYCODE_Y, false);
		Key(context, Platform::Input::KEYCODE_LCTRL, false);
		frame();
		Check(instances().size() == 1 && instances()[0] == placed[0], "Redo restores the placed entity's identity");
		source = sourceIn(context);
		Pointer(context, Platform::EventType::MouseButton, center(*source), true);
		Pointer(context, Platform::EventType::MouseMove, target);
		Key(context, Platform::Input::KEYCODE_ESCAPE);
		Pointer(context, Platform::EventType::MouseButton, target);
		frame();
		Check(instances().size() == 1, "Escape cancels placement without creating an entity");
		source = sourceIn(context);
		Pointer(context, Platform::EventType::MouseButton, center(*source), true);
		Pointer(context, Platform::EventType::MouseMove, target);
		Pointer(context, Platform::EventType::MouseMove, {-10, -10});
		Pointer(context, Platform::EventType::MouseButton, {-10, -10});
		frame();
		Check(instances().size() == 1, "Dropping outside the scene cancels placement");
		// A detached source uses its own DPI and native client-to-screen mapping.
		editor.GetWorkspace()->Detach("assets");
		frame();
		Check(!editor.GetWorkspace()->GetWindows().empty(), "Asset browser detaches");
		auto& window = *editor.GetWorkspace()->GetWindows().front();
		window.GetWindow().Display(false);
		auto& floating = window.GetContext();
		floating.SetViewport(900, 650, 1.25f);
		floating.Layout();
		source = sourceIn(floating);
		int screenX = int(std::lround(target.x * context.GetScale()));
		int screenY = int(std::lround(target.y * context.GetScale()));
		Check(engine.GetMainWindow()->ClientToScreen(screenX, screenY) && window.GetWindow().ScreenToClient(screenX, screenY), "Native drag coordinates convert");
		const UI::Vec2 nativeTarget{screenX / floating.GetScale(), screenY / floating.GetScale()};
		Pointer(floating, Platform::EventType::MouseButton, center(*source), true);
		Pointer(floating, Platform::EventType::MouseMove, nativeTarget);
		frame();
		Pointer(floating, Platform::EventType::MouseButton, nativeTarget);
		frame();
		Check(instances().size() == 2, "Detached browser drops a mesh using its own DPI");
		source = sourceIn(floating);
		Pointer(floating, Platform::EventType::MouseButton, center(*source), true);
		Pointer(floating, Platform::EventType::MouseMove, nativeTarget);
		Platform::Event focus;
		focus.mType = Platform::EventType::FocusChanged;
		focus.mFocused = false;
		floating.ProcessEvent(focus);
		frame();
		Check(instances().size() == 2 && !floating.GetCapturedWidget(), "Losing source focus cancels mesh placement");
		focus.mFocused = true;
		floating.ProcessEvent(focus);
		source = sourceIn(floating);
		Pointer(floating, Platform::EventType::MouseButton, center(*source), true);
		Pointer(floating, Platform::EventType::MouseButton, center(*source));
		frame();
		const auto document = "asset_" + std::to_string(static_cast<int>(Assets::AssetType::Mesh)) + "_" + mesh.GetUUID().str();
		Check(editor.GetWorkspace()->Contains(document) && instances().size() == 2, "A click still opens the mesh viewer without placing an entity");
		editor.Shutdown();
		std::cout << "Asset dragging, ghost depth, placement, undo/redo, cancellation and detached DPI passed.\n";
	}
}
