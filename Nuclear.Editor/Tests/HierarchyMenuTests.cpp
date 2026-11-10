#include "HierarchyMenuTests.h"
#include <NuclearEditor.h>
#include <EditorHierarchyRow.h>
#include <EditorMenuPopup.h>
#include <Nuclear.Platform/Input.h>
#include <iostream>
#include <stdexcept>
#include <cmath>

namespace Nuclear::Editor::Tests
{
	namespace
	{
		void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
		template<class T, class Predicate> T* Find(UI::UIContext& context, Predicate predicate)
		{
			UI::Widget marker;
			for (UI::WidgetId id = 1; id < marker.GetId(); ++id)
				if (auto* item = dynamic_cast<T*>(context.Find(id)); item && predicate(*item)) return item;
			return nullptr;
		}
		void Pointer(UI::UIContext& context, UI::Vec2 point, int button, bool down)
		{
			Platform::Event event;
			event.mType = Platform::EventType::MouseButton;
			event.mButton = button;
			event.mPressed = down;
			event.mMouseX = int(std::lround(point.x * context.GetScale()));
			event.mMouseY = int(std::lround(point.y * context.GetScale()));
			context.ProcessEvent(event);
		}
		void Click(UI::UIContext& context, UI::Widget& widget, int button = MOUSE_BUTTON_LEFT)
		{
			context.Layout();
			for (auto* parent = widget.GetParent(); parent; parent = parent->GetParent())
				if (auto* scroll = dynamic_cast<UI::ScrollBox*>(parent)) scroll->ScrollIntoView(widget);
			context.Layout();
			const auto bounds = widget.GetBounds();
			const UI::Vec2 point{bounds.x + bounds.Width / 2, bounds.y + bounds.Height / 2};
			Pointer(context, point, button, true);
			Pointer(context, point, button, false);
		}
		void Key(UI::UIContext& context, Platform::Input::KeyCode key, bool down = true)
		{
			Platform::Event event;
			event.mType = Platform::EventType::Key;
			event.mKey = key;
			event.mPressed = down;
			context.ProcessEvent(event);
		}
		void Shortcut(UI::UIContext& context, Platform::Input::KeyCode key)
		{
			Key(context, Platform::Input::KEYCODE_LCTRL);
			Key(context, key);
			Key(context, key, false);
			Key(context, Platform::Input::KEYCODE_LCTRL, false);
		}
		entt::entity Entity(const std::string& name)
		{
			auto& registry = Core::Scene::Get().GetRegistry();
			for (auto entity : registry.view<Components::EntityInfoComponent>())
				if (registry.get<Components::EntityInfoComponent>(entity).mName == name) return entity;
			return entt::null;
		}
		void Command(UI::UIContext& context, const std::string& title)
		{
			auto* label = Find<UI::TextBlock>(context, [&](const auto& text) { return text.IsVisible() && text.GetText() == title; });
			Check(label && dynamic_cast<UI::Button*>(label->GetParent()), "Hierarchy menu command is present");
			Click(context, *label->GetParent());
		}
		void Open(UI::UIContext& context, const std::string& name)
		{
			auto* row = Find<EditorHierarchyRow>(context, [&](const auto& item) {
				if (!item.IsVisible()) return false;
				UI::Widget marker;
				for (UI::WidgetId id = item.GetId() + 1; id < marker.GetId(); ++id)
					if (auto* label = dynamic_cast<UI::TextBlock*>(context.Find(id)); label && label->GetParent() == &item)
						return label->GetText().substr(2) == name;
				return false;
			});
			Check(row != nullptr, "Hierarchy entity row exists");
			Click(context, *row, MOUSE_BUTTON_RIGHT);
			context.Layout();
			Check(Find<EditorMenuPopup>(context, [](const auto&) { return true; }) != nullptr, "Right click opens an entity menu");
		}
	}

	void RunHierarchyMenuTests()
	{
		NuclearEditor editor;
		editor.Load();
		auto& context = UI::UIModule::Get().GetContext();
		auto& registry = Core::Scene::Get().GetRegistry();
		auto frame = [&]() {
			auto& engine = Core::Engine::Get();
			engine.BeginFrame();
			editor.Update(1.f / 60);
			editor.Render(1.f / 60);
			engine.EndFrame();
		};
		entt::entity source = entt::null;
		for (auto entity : registry.view<Components::MeshComponent>()) { source = entity; break; }
		Check(source != entt::null, "Initial authoring mesh exists");
		registry.get<Components::EntityInfoComponent>(source).mName = "Hierarchy Source";
		registry.get<Components::EntityInfoComponent>(source).mTransform.SetPosition({1, 2, 3});
		registry.get<Components::EntityInfoComponent>(source).mTransform.SetScale({1, 2, 1});
		registry.get<Components::MeshComponent>(source).SetCastShadow(false);
		frame();
		Open(context, "Hierarchy Source");
		frame();
		Check(Find<EditorMenuPopup>(context, [](const auto&) { return true; }) != nullptr,
			"Hierarchy rebuilding preserves the independently owned menu");
		Command(context, "Duplicate");
		frame();
		const auto copy = Entity("Hierarchy Source Copy");
		Check(copy != entt::null && copy != source, "Duplicate creates a fresh entity identity");
		Check(registry.get<Components::EntityInfoComponent>(copy).mTransform.GetLocalPosition() == Math::Vector3(1, 2, 3) &&
			registry.get<Components::EntityInfoComponent>(copy).mTransform.GetLocalScale() == Math::Vector3(1, 2, 1), "Duplicate preserves authored transform");
		Check(registry.get<Components::MeshComponent>(copy).GetMesh() == registry.get<Components::MeshComponent>(source).GetMesh() &&
			!registry.get<Components::MeshComponent>(copy).GetCastShadow(), "Duplicate borrows geometry and preserves mesh flags");
		Shortcut(context, Platform::Input::KEYCODE_Z);
		frame();
		Check(!registry.valid(copy) && registry.valid(source), "One undo removes only the duplicate");
		Shortcut(context, Platform::Input::KEYCODE_Y);
		frame();
		Check(Entity("Hierarchy Source Copy") == copy, "Redo restores duplicate identity");
		Open(context, "Hierarchy Source");
		Command(context, "Copy");
		frame();
		registry.get<Components::EntityInfoComponent>(source).mTransform.SetPosition({9, 9, 9});
		Open(context, "Hierarchy Source");
		Command(context, "Delete");
		frame();
		Check(!registry.valid(source) && registry.valid(copy), "Delete targets the right-clicked row rather than the previous selection");
		Shortcut(context, Platform::Input::KEYCODE_V);
		frame();
		const auto pasted = Entity("Hierarchy Source Copy 2");
		Check(pasted != entt::null && registry.get<Components::EntityInfoComponent>(pasted).mTransform.GetLocalPosition() == Math::Vector3(1, 2, 3),
			"Paste uses the copied snapshot after source edits/deletion and chooses a unique name");
		Shortcut(context, Platform::Input::KEYCODE_Z);
		frame();
		Shortcut(context, Platform::Input::KEYCODE_Z);
		frame();
		Check(registry.valid(source) && !registry.valid(pasted), "Copy creates no extra undo entry; delete restores the source identity");
		Open(context, "Key Light");
		Command(context, "Duplicate");
		frame();
		const auto lightCopy = Entity("Key Light Copy");
		Check(lightCopy != entt::null && registry.all_of<Components::LightComponent>(lightCopy), "Context duplicate copies the clicked light component");
		Open(context, "Hierarchy Source");
		Key(context, Platform::Input::KEYCODE_ESCAPE);
		Check(!Find<EditorMenuPopup>(context, [](const auto&) { return true; }), "Escape dismisses hierarchy commands");
		editor.GetWorkspace()->Detach("hierarchy");
		frame();
		Check(!editor.GetWorkspace()->GetWindows().empty(), "Hierarchy detaches for native context tests");
		auto& window = *editor.GetWorkspace()->GetWindows().front();
		window.GetWindow().Display(false);
		auto& floating = window.GetContext();
		floating.SetViewport(850, 650, 1.25f);
		Open(floating, "Hierarchy Source");
		frame();
		Command(floating, "Duplicate");
		frame();
		Check(Entity("Hierarchy Source Copy 2") != entt::null, "Detached hierarchy executes context commands at its DPI");
		Open(floating, "Hierarchy Source");
		editor.GetWorkspace()->Dock("hierarchy", EditorDockRegion::Left);
		frame();
		Check(editor.GetWorkspace()->GetWindows().empty() && !Find<EditorMenuPopup>(context, [](const auto&) { return true; }),
			"Redocking with an open menu safely discards the native popup context");
		Open(context, "Hierarchy Source");
		Command(context, "Copy");
		frame();
		auto* nameInput = Find<UI::TextBox>(context, [](const auto& input) { return input.IsVisible() && input.GetText() == "Hierarchy Source"; });
		Check(nameInput != nullptr, "Selected entity has an inspector name input");
		context.SetFocus(nameInput->GetId());
		Key(context, Platform::Input::KEYCODE_LCTRL);
		Platform::Event paste;
		paste.mType = Platform::EventType::Key;
		paste.mKey = Platform::Input::KEYCODE_V;
		paste.mPressed = true;
		Check(!context.OnPreviewEvent(paste), "Focused text inputs retain their clipboard shortcuts");
		Key(context, Platform::Input::KEYCODE_LCTRL, false);
		context.SetFocus(0);
		Command(context, "Edit");
		Command(context, "Paste");
		frame();
		Check(Entity("Hierarchy Source Copy 3") != entt::null, "Edit menu Paste uses the session clipboard");
		auto* newScene = Find<UI::TextBlock>(context, [](const auto& text) { return text.IsVisible() && text.GetText() == "New scene"; });
		Check(newScene != nullptr, "New scene action exists");
		Open(context, "Hierarchy Source");
		dynamic_cast<UI::Button*>(newScene->GetParent())->OnClicked();
		frame();
		Check(!Find<EditorMenuPopup>(context, [](const auto&) { return true; }), "Scene replacement closes hierarchy menus");
		Shortcut(context, Platform::Input::KEYCODE_V);
		frame();
		Check(Entity("Hierarchy Source Copy") == entt::null, "New scenes clear the session entity clipboard");
		editor.Shutdown();
		std::cout << "Hierarchy context selection, duplicate/copy/delete, paste, history and detached menu lifetime passed.\n";
	}
}
