#include "ComponentInspectorTests.h"
#include <EntityEditor.h>
#include <EditorComponentButton.h>
#include <EditorHistory.h>
#include <EditorColorPickerWindow.h>
#include <SkyboxComponent.h>
#include <iostream>
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

		TextBlock* Label(UIContext& context, const std::string& title)
		{
			return Find<TextBlock>(context, [&](const auto& label) { return label.GetText() == title; });
		}

		EditorComponentButton& HeaderButton(UIContext& context, const std::string& title, EditorComponentButton::Action action)
		{
			auto* heading = Label(context, title);
			Check(heading != nullptr, "Component heading exists");
			auto* button = Find<EditorComponentButton>(context, [&](const auto& button) {
				return button.GetParent() == heading->GetParent() && button.GetAction() == action;
			});
			Check(button != nullptr, "Component heading has disclosure and removal controls");
			return *button;
		}

		void Click(Widget& widget)
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
			event.mMouseX = int(bounds.x + bounds.Width * .5f);
			event.mMouseY = int(bounds.y + bounds.Height * .5f);
			event.mButton = MOUSE_BUTTON_LEFT;
			event.mPressed = true;
			context.ProcessEvent(event);
			event.mPressed = false;
			context.ProcessEvent(event);
		}
	}

	void RunComponentInspectorTests()
	{
		auto& scene = Core::Scene::Get();
		auto& registry = scene.GetRegistry();
		const auto entity = scene.CreateEntity("Component inspector fixture").GetEntityID();
		const auto other = scene.CreateEntity("Other inspector fixture").GetEntityID();
		EntityEditor inspector;
		EditorHistory history;
		EditorColorPickerWindow colors;
		UIContext context;
		context.SetViewport(300, 480);
		context.SetDefaultFont(UIModule::Get().GetContext().GetDefaultFont());
		bool structuralChange = false;
		auto build = [&](entt::entity selected = entt::null) {
			context.Clear();
			context.SetDefaultFont(UIModule::Get().GetContext().GetDefaultFont());
			auto scroll = std::make_unique<ScrollBox>();
			scroll->SetContent(inspector.Build(selected == entt::null ? entity : selected,
				[&](bool structural) { structuralChange = structural; }, history, colors));
			context.AddToViewport(std::move(scroll));
			context.Layout();
		};
		using Action = EditorComponentButton::Action;
		build();
		auto& transformRemove = HeaderButton(context, "TRANSFORM", Action::Remove);
		Check(!transformRemove.IsEnabled(), "Required transform cannot be removed");
		const auto beforeCollapse = EditorEntityState::Capture(entity);
		Click(HeaderButton(context, "TRANSFORM", Action::Collapse));
		Check(!Label(context, "POSITION")->IsVisible() && registry.valid(entity), "Disclosure hides properties without removing the entity");
		Check(EditorEntityState::Capture(entity) == beforeCollapse && !history.CanUndo(), "Disclosure changes no authored data or history");
		build();
		Check(!Label(context, "POSITION")->IsVisible(), "Disclosure survives inspector rebuilds");
		build(other);
		Check(Label(context, "POSITION")->IsVisible(), "Disclosure is independent for another entity");
		build();
		Check(!Label(context, "POSITION")->IsVisible(), "Returning to an entity retains its disclosure state");
		context.SetFocus(HeaderButton(context, "TRANSFORM", Action::Collapse).GetId());
		Platform::Event key;
		key.mType = Platform::EventType::Key;
		key.mKey = Platform::Input::KEYCODE_RETURN;
		key.mPressed = true;
		context.ProcessEvent(key);
		key.mPressed = false;
		context.ProcessEvent(key);
		Check(Label(context, "POSITION")->IsVisible(), "Keyboard activation expands a component");

		registry.emplace<Components::LightComponent>(entity).SetIntensity(7);
		registry.emplace<Components::MeshComponent>(entity).SetEnableRendering(false);
		registry.emplace<Components::RigidBodyComponent>(entity).isKinematic = true;
		registry.emplace<Components::ColliderComponent>(entity);
		registry.emplace<Components::ScriptComponent>(entity);
		registry.emplace<Components::AudioListenerComponent>(entity);
		registry.emplace<Components::AudioSourceComponent>(entity);
		Components::CameraComponentDesc camera;
		camera.mInitRTs = false;
		camera.mIsActiveCamera = false;
		camera.mProjection = Math::Matrix4(1);
		registry.emplace<Components::CameraComponent>(entity, camera);
		registry.emplace<SkyboxComponent>(entity).Enabled = false;
		const auto authored = EditorEntityState::Capture(entity);
		auto remove = [&]<class T>(const char* title) {
			build();
			structuralChange = false;
			const auto heading = Label(context, title)->GetBounds();
			const auto collapse = HeaderButton(context, title, Action::Collapse).GetBounds();
			const auto close = HeaderButton(context, title, Action::Remove).GetBounds();
			Check(collapse.x + collapse.Width <= heading.x && heading.x + heading.Width <= close.x,
				"Disclosure precedes the component title and X stays on its right");
			Click(HeaderButton(context, title, Action::Collapse));
			Click(HeaderButton(context, title, Action::Remove));
			Check(registry.valid(entity) && !registry.all_of<T>(entity) && structuralChange,
				"X removes a collapsed component and requests renderer/inspector refresh");
			build();
			Check(!Label(context, title), "Removed component no longer has an inspector section");
			Check(history.Undo() && registry.all_of<T>(entity) && EditorEntityState::Capture(entity) == authored,
				"Undo restores removed component presence and authored values");
			Check(history.Redo() && !registry.all_of<T>(entity), "Redo removes the component again");
			Check(history.Undo(), "Restore component for the next removal check");
			history.Clear();
		};
		remove.operator()<Components::LightComponent>("LIGHT");
		remove.operator()<Components::MeshComponent>("MESH");
		remove.operator()<Components::RigidBodyComponent>("RIGID BODY");
		remove.operator()<Components::ColliderComponent>("COLLIDER");
		remove.operator()<Components::CameraComponent>("CAMERA");
		remove.operator()<Components::ScriptComponent>("SCRIPT");
		remove.operator()<Components::AudioListenerComponent>("AUDIO LISTENER");
		remove.operator()<Components::AudioSourceComponent>("AUDIO SOURCE");
		remove.operator()<SkyboxComponent>("SKYBOX");
		Check(!registry.get<Components::CameraComponent>(entity).GetIsActive() &&
			registry.get<Components::AudioSourceComponent>(entity).GetSourceID() == 0,
			"Restoring removed runtime-service components keeps them inactive");
		build();
		Click(HeaderButton(context, "LIGHT", Action::Remove));
		build();
		auto* addButton = Label(context, "Add component  v");
		Click(*addButton->GetParent());
		auto* lightOption = Label(context, "Light");
		Check(lightOption && lightOption->IsVisible(), "A removed component returns to the Add component menu");
		Click(*lightOption->GetParent());
		Check(registry.all_of<Components::LightComponent>(entity), "Removed component can be added again");
		build();
		Check(!Label(context, "Intensity")->IsVisible(), "Component disclosure survives structural rebuilds");
		inspector.Reset();
		build();
		Check(Label(context, "Intensity")->IsVisible(), "Scene reset clears component disclosure state");

		auto* pointOption = Label(context, "Point");
		auto* spotOption = Label(context, "Spot");
		auto* dirOption = Label(context, "Directional");
		Check(pointOption && spotOption && dirOption, "Light inspector provides a segmented button for light types");
		Click(*spotOption->GetParent());
		Check(registry.get<Components::LightComponent>(entity).GetType() == Components::LightComponent::Type::Spot,
			"Segmented button switches light type to Spot");
		Check(history.Undo() && registry.get<Components::LightComponent>(entity).GetType() != Components::LightComponent::Type::Spot,
			"Undo restores previous light type");
		Check(history.Redo() && registry.get<Components::LightComponent>(entity).GetType() == Components::LightComponent::Type::Spot,
			"Redo reapplies light type selection");

		const auto staleRemove = HeaderButton(context, "LIGHT", Action::Remove).OnClicked;
		registry.destroy(entity);
		const auto replacement = scene.CreateEntity("Replacement inspector fixture").GetEntityID();
		registry.emplace<Components::LightComponent>(replacement);
		history.Clear();
		staleRemove();
		Check(registry.all_of<Components::LightComponent>(replacement) && !history.CanUndo(),
			"A stale removal callback cannot edit a new entity reusing the old slot");
		context.Clear();
		registry.destroy(replacement);
		registry.destroy(other);
		std::cout << "Component inspector disclosure, removal, required transform and undo/redo passed.\n";
	}
}
