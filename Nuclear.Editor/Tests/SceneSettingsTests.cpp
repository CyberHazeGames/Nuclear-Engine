#include "SceneSettingsTests.h"
#include <NuclearEditor.h>
#include <SceneSettings.h>
#include <EditorSceneState.h>
#include <cmath>
#include <stdexcept>

namespace Nuclear::Editor::Tests
{
	namespace
	{
		using namespace UI;

		void Check(bool condition, const char* message)
		{
			if (!condition)
				throw std::runtime_error(message);
		}

		template<class T, class Predicate> T* Find(UIContext& context, Predicate predicate)
		{
			Widget marker;
			for (WidgetId id = 1; id < marker.GetId(); ++id)
				if (auto* widget = dynamic_cast<T*>(context.Find(id)); widget && predicate(*widget))
					return widget;
			return nullptr;
		}

		void Send(NuclearEditor& editor, UIContext& context, Platform::Event event)
		{
			if (&context == &UIModule::Get().GetContext())
				context.ProcessEvent(event);
			else
				for (const auto& window : editor.GetWorkspace()->GetWindows())
					if (&window->GetContext() == &context)
					{
						event.pWindow = &window->GetWindow();
						editor.OnAuxiliaryWindowEvent(event);
						return;
					}
		}

		void Key(NuclearEditor& editor, UIContext& context, Platform::Input::KeyCode key, bool down = true)
		{
			Platform::Event event;
			event.mType = Platform::EventType::Key;
			event.mKey = key;
			event.mPressed = down;
			Send(editor, context, event);
		}

		void Click(NuclearEditor& editor, Widget& widget)
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
			Platform::Event event;
			event.mType = Platform::EventType::MouseButton;
			event.mMouseX = int((bounds.x + bounds.Width * .5f) * context.GetScale());
			event.mMouseY = int((bounds.y + bounds.Height * .5f) * context.GetScale());
			event.mButton = MOUSE_BUTTON_LEFT;
			event.mPressed = true;
			Send(editor, context, event);
			event.mPressed = false;
			Send(editor, context, event);
		}

		void Action(NuclearEditor& editor, const std::string& text)
		{
			auto* label = Find<TextBlock>(UIModule::Get().GetContext(), [&](const auto& label) {
				return label.IsVisible() && label.GetText() == text;
			});
			Check(label && label->GetParent(), "Scene settings action exists");
			Click(editor, *label->GetParent());
		}

		TextBox& Input(SceneSettings& panel, const std::string& title)
		{
			auto* label = Find<TextBlock>(*panel.GetContext(), [&](const auto& label) {
				return label.GetText() == title && label.IsVisible();
			});
			Check(label && label->GetParent(), "Scene settings property label exists");
			// Stable IDs follow creation order: each property label is followed by its text field.
			auto* input = Find<TextBox>(*panel.GetContext(), [label](const auto& input) {
				return input.GetParent() == label->GetParent() && input.GetId() > label->GetId();
			});
			Check(input != nullptr, "Scene settings label has a text field");
			return *input;
		}

		void Edit(NuclearEditor& editor, SceneSettings& panel, const std::string& title, const std::string& text)
		{
			auto& input = Input(panel, title);
			auto& context = *panel.GetContext();
			Click(editor, input);
			Key(editor, context, Platform::Input::KEYCODE_LCTRL);
			Key(editor, context, Platform::Input::KEYCODE_A);
			Key(editor, context, Platform::Input::KEYCODE_LCTRL, false);
			for (unsigned char character : text)
			{
				Platform::Event event;
				event.mType = Platform::EventType::TextInput;
				event.mCodepoint = character;
				Send(editor, context, event);
			}
			Key(editor, context, Platform::Input::KEYCODE_RETURN);
			Key(editor, context, Platform::Input::KEYCODE_RETURN, false);
		}

		void CheckClearPixel()
		{
			using namespace Diligent;
			auto* source = Core::Scene::Get().GetMainCamera()->GetColorRT().GetRTV()->GetTexture();
			auto desc = source->GetDesc();
			desc.Usage = USAGE_STAGING;
			desc.BindFlags = BIND_NONE;
			desc.CPUAccessFlags = CPU_ACCESS_READ;
			auto& graphics = Graphics::GraphicsModule::Get();
			RefCntAutoPtr<ITexture> staging;
			graphics.GetDevice()->CreateTexture(desc, nullptr, &staging);
			Check(staging != nullptr, "Scene clear readback allocation");
			auto* context = graphics.GetContext();
			CopyTextureAttribs copy;
			copy.pSrcTexture = source;
			copy.pDstTexture = staging;
			copy.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
			copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
			context->CopyTexture(copy);
			context->WaitForIdle();
			MappedTextureSubresource mapped;
			context->MapTextureSubresource(staging, 0, 0, MAP_READ, MAP_FLAG_NONE, nullptr, mapped);
			Check(mapped.pData != nullptr, "Scene clear readback mapping");
			const auto* pixel = static_cast<const unsigned char*>(mapped.pData);
			const bool red = pixel[0] > 250 && pixel[1] < 5 && pixel[2] < 5;
			context->UnmapTextureSubresource(staging, 0, 0);
			Check(red, "Scene clear color reaches the rendered camera target");
		}
	}

	void RunSceneSettingsTests(NuclearEditor& editor, const std::function<void()>& frame)
	{
		auto* workspace = editor.GetWorkspace();
		workspace->Dock("inspector", EditorDockRegion::Right);
		workspace->Show("viewport");
		workspace->Close("scene-settings");
		frame();
		Check(workspace->GetArea("scene-settings") == nullptr, "Closed Scene settings can be reopened from the menu");
		Action(editor, "Window");
		Action(editor, "Scene settings");
		frame();
		auto* panel = Find<SceneSettings>(UIModule::Get().GetContext(), [](const auto&) { return true; });
		Check(panel && panel->IsVisible(), "Window menu opens a docked Scene settings panel");
		const auto panelId = panel->GetId();
		const auto original = EditorSceneState::Capture();
		Edit(editor, *panel, "Scene name", "Settings Test Scene");
		frame();
		Check(Core::Scene::Get().GetSceneAsset()->GetName() == "Settings Test Scene", "Scene name commits to the active scene asset");
		auto history = [&](const char* command) {
			Action(editor, "Edit");
			Action(editor, command);
			frame();
		};
		history("Undo");
		Check(EditorSceneState::Capture() == original && Input(*panel, "Scene name").GetText() == original.Name,
			"Scene-name undo refreshes both scene data and the settings control");
		history("Redo");
		Check(Input(*panel, "Scene name").GetText() == "Settings Test Scene", "Scene-name redo refreshes settings");
		Action(editor, "Add");
		Action(editor, "Empty Entity");
		frame();
		entt::entity fixture = entt::null;
		for (auto entity : Core::Scene::Get().GetRegistry().view<Components::EntityInfoComponent>())
			if (Core::Scene::Get().GetRegistry().get<Components::EntityInfoComponent>(entity).mName == "Entity")
				fixture = entity;
		Check(fixture != entt::null, "Mixed scene/entity history fixture exists");
		Edit(editor, *panel, "Scene name", "Settings Test Scene 2");
		frame();
		history("Undo");
		Check(Core::Scene::Get().GetRegistry().valid(fixture) && EditorSceneState::Capture().Name == "Settings Test Scene",
			"Scene undo preserves entities while following the shared transaction order");
		workspace->Show("inspector");
		frame();
		Check(Find<TextBox>(UIModule::Get().GetContext(), [](const auto& input) {
			return input.IsVisible() && input.GetText() == "Entity";
		}), "Undoing scene settings preserves entity selection");
		history("Undo");
		Check(!Core::Scene::Get().GetRegistry().valid(fixture), "The next undo reaches the earlier entity action");
		history("Redo");
		history("Redo");
		workspace->Show("scene-settings");
		frame();
		Edit(editor, *panel, "Red", "1");
		frame();
		Edit(editor, *panel, "Green", "0");
		frame();
		Edit(editor, *panel, "Blue", "0");
		frame();
		CheckClearPixel();
		for (const char* invalid : {"nan", "1.5", "-0.1", "abc"})
		{
			Edit(editor, *panel, "Red", invalid);
			frame();
			Check(EditorSceneState::Capture().ClearColor.r == 1, "Invalid and out-of-range color edits leave the scene unchanged");
		}
		history("Undo");
		Check(EditorSceneState::Capture().ClearColor.b == original.ClearColor.b, "Invalid fields do not add undo entries");
		Edit(editor, *panel, "Green", "0");
		frame();
		history("Redo");
		Check(EditorSceneState::Capture().ClearColor.b == 0, "No-op settings edits preserve redo");
		Edit(editor, *panel, "Alpha", "0.5");
		frame();
		Check(EditorSceneState::Capture().ClearColor.a == .5f, "Alpha is an editable clear-color channel");
		history("Undo");
		Check(EditorSceneState::Capture().ClearColor.a == 1, "Alpha edits participate in scene history");
		workspace->Detach("scene-settings");
		frame();
		Check(workspace->GetWindows().size() == 1 && panel->GetContext() != &UIModule::Get().GetContext(),
			"Scene settings detaches with its original widget identity");
		workspace->GetWindows().front()->GetWindow().Display(false);
		Edit(editor, *panel, "Blue", "0.25");
		frame();
		Check(EditorSceneState::Capture().ClearColor.b == .25f, "Detached scene controls update the same scene");
		auto& floating = *panel->GetContext();
		Key(editor, floating, Platform::Input::KEYCODE_LCTRL);
		Key(editor, floating, Platform::Input::KEYCODE_Z);
		Key(editor, floating, Platform::Input::KEYCODE_LCTRL, false);
		frame();
		Check(EditorSceneState::Capture().ClearColor.b == 0, "Detached settings use shared scene undo");
		Action(editor, "New project");
		frame();
		Check(!panel->IsEnabled(), "Project modal disables detached scene settings");
		Action(editor, "Cancel");
		frame();
		const auto previous = EditorSceneState::Capture();
		Action(editor, "New scene");
		frame();
		Check(!previous.Restore() && EditorSceneState::Capture().Name == "Untitled Scene" &&
			EditorSceneState::Capture().ClearColor == original.ClearColor &&
			Input(*panel, "Scene name").GetText() == "Untitled Scene",
			"A new scene resets settings and rejects snapshots of the old scene");
		workspace->Close("scene-settings");
		frame();
		Check(workspace->GetArea("scene-settings") == nullptr && workspace->GetWindows().empty(),
			"Closing detached settings parks the panel and releases its window");
		Action(editor, "View");
		Action(editor, "Scene settings");
		frame();
		Check(panel->GetId() == panelId && panel->GetContext() == &UIModule::Get().GetContext(),
			"View menu reopens the same settings panel in the viewport workspace");
		workspace->Detach("scene-settings");
		frame();
		workspace->GetWindows().front()->GetWindow().Display(false);
	}
}
