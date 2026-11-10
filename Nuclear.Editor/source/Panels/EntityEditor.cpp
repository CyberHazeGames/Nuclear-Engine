#include <EntityEditor.h>
#include <EditorSettings.h>
#include <EditorWidgets.h>
#include <EditorHistory.h>
#include <SkyboxComponent.h>
#include <EditorComponentButton.h>
#include <EditorColorProperty.h>
#include <EditorAssetProperty.h>
#include <cmath>
namespace Nuclear::Editor
{
	namespace
	{
		template <class T> T* Component(entt::entity entity)
		{
			auto& registry = Core::Scene::Get().GetRegistry();
			return registry.valid(entity) ? registry.try_get<T>(entity) : nullptr;
		}
		void Toggle(UI::PanelWidget& parent, const std::string& title, bool value,
					std::function<void(bool)> setter)
		{
			auto& check = parent.Add<UI::CheckBox>();
			check.SetChecked(value);
			Label(check, title);
			check.OnChanged = std::move(setter);
		}
		template <class T>
		void AddComponent(UI::PanelWidget& parent, entt::entity entity, const std::string& title,
						  const std::function<void(bool)>& changed, EditorHistory& history)
		{
			if (Component<T>(entity))
				return;
			Action(parent, title, [entity, changed, &history]() {
				auto& registry = Core::Scene::Get().GetRegistry();
				history.Edit(entity, "Add component", [&]() {
					if (!registry.all_of<T>(entity))
						registry.emplace<T>(entity);
				});
				changed(true);
			});
		}
	} // namespace
	void EntityEditor::Reset()
	{
		mCollapsedComponents.clear();
		mAssetPreviews.Clear();
	}

	UI::VerticalBox& EntityEditor::ComponentSection(UI::PanelWidget& parent, entt::entity entity,
		entt::id_type component, const std::string& title, std::function<void()> remove)
	{
		auto& border = parent.Add<UI::Border>();
		border.SetBrush({nullptr, {.06f, .075f, .095f, 1}});
		border.SetPadding({0, 0, 0, 0});

		auto& section = border.Add<UI::VerticalBox>();
		section.SetSpacing(10);

		auto& header = section.Add<UI::HorizontalBox>();
		header.SetSpacing(3);
		auto& collapse = header.Add<EditorComponentButton>(EditorComponentButton::Action::Collapse);
		auto& heading = Label(header, title, 14);
		heading.SetColor(EditorSettings::Get().GetAccentColor());
		Fill(header, heading);
		auto slot = header.GetSlot(heading.GetId());
		slot.Vertical = UI::Alignment::Center;
		header.SetSlot(heading.GetId(), slot);

		auto& close = header.Add<EditorComponentButton>(EditorComponentButton::Action::Remove);
		close.SetEnabled(bool(remove));
		close.OnClicked = std::move(remove);

		auto& body = section.Add<UI::VerticalBox>();
		body.SetSpacing(10);
		const bool expanded = !mCollapsedComponents[entity].contains(component);
		collapse.SetExpanded(expanded);
		body.SetVisibility(expanded ? UI::Visibility::Visible : UI::Visibility::Collapsed);

		collapse.OnClicked = [this, entity, component, &body, &collapse]() {
			const bool expanded = body.GetVisibility() == UI::Visibility::Collapsed;
			if (expanded)
				mCollapsedComponents[entity].erase(component);
			else
				mCollapsedComponents[entity].insert(component);
			body.SetVisibility(expanded ? UI::Visibility::Visible : UI::Visibility::Collapsed);
			collapse.SetExpanded(expanded);
		};

		return body;
	}

	std::unique_ptr<UI::Widget> EntityEditor::Build(entt::entity entity, std::function<void(bool)> changed,
		EditorHistory& history, EditorColorPickerWindow& colors)
	{
		// Wrap every property setter at the commit boundary, never on text-change or paint.
		auto number = [entity, changed, &history](UI::PanelWidget& parent, const std::string& title,
			float value, std::function<void(float)> setter, float minimum = -100000, float maximum = 100000) {
			Number(parent, title, value, [entity, changed, &history, setter](float value) {
				history.Edit(entity, "Edit property", [&]() { setter(value); });
				changed(false);
			}, minimum, maximum);
		};
		auto toggle = [entity, changed, &history](UI::PanelWidget& parent, const std::string& title,
			bool value, std::function<void(bool)> setter) {
			Toggle(parent, title, value, [entity, changed, &history, setter](bool value) {
				history.Edit(entity, "Edit property", [&]() { setter(value); });
				changed(false);
			});
		};
		auto vector = [entity, changed, &history](UI::PanelWidget& parent, const std::string& title,
			Math::Vector3 value, std::function<void(int, float)> setter, float minimum = -100000, float maximum = 100000,
			float labelWidth = 80.0f) {
			Vector(parent, title, value, [entity, changed, &history, setter](int axis, float value) {
				history.Edit(entity, "Edit property", [&]() { setter(axis, value); });
				changed(false);
			}, minimum, maximum, labelWidth);
		};
		auto root = std::make_unique<UI::VerticalBox>();
		root->SetSpacing(12);
		auto componentSection = [this, &root, entity, changed, &history]<class T>(const std::string& title)
			-> UI::VerticalBox& {
			return ComponentSection(*root, entity, entt::type_hash<T>::value(), title, [entity, changed, &history]() {
				history.Edit(entity, "Remove component", [entity]() {
					auto& registry = Core::Scene::Get().GetRegistry();
					if (registry.valid(entity))
						registry.remove<T>(entity);
				});
				changed(true);
			});
		};
		auto* info = Component<Components::EntityInfoComponent>(entity);
		if (!info)
		{
			Label(*root, "Select an entity in the hierarchy.").SetWrap(true);
			return root;
		}
		Label(*root, "ENTITY / " + std::to_string(static_cast<Uint32>(entity)), 13);
		auto& name = root->Add<UI::TextBox>(info->mName);
		name.OnCommitted = [entity, changed, &history](const std::string& value) {
			history.Edit(entity, "Rename entity", [&]() {
				if (auto* info = Component<Components::EntityInfoComponent>(entity))
					info->mName = value.empty() ? "Entity" : value;
			});
			changed(false);
		};
		auto& componentMenu = root->Add<UI::VerticalBox>();
		componentMenu.SetSpacing(4);
		auto& componentButton = Action(componentMenu, "Add component  v", {});
		auto& componentList = componentMenu.Add<UI::Border>();
		componentList.SetBrush({nullptr, {.06f, .075f, .095f, 1}});
		componentList.SetPadding({4, 4, 4, 4});
		componentList.SetVisibility(UI::Visibility::Collapsed);
		auto& add = componentList.Add<UI::VerticalBox>();
		add.SetSpacing(2);
		componentButton.OnClicked = [&componentList]() {
			componentList.SetVisibility(componentList.GetVisibility() == UI::Visibility::Collapsed
										? UI::Visibility::Visible
											: UI::Visibility::Collapsed);
		};
		if (!Component<SkyboxComponent>(entity))
		{
			auto& transform = ComponentSection(*root, entity,
				entt::type_hash<Components::EntityInfoComponent>::value(), "TRANSFORM", {});
			Label(transform, "Required by every entity. Enter commits a value. Rotation is in degrees.", 12).SetWrap(true);
			for (int group = 0; group < 3; ++group)
			{
				const char* names[] = {"POSITION", "ROTATION", "SCALE"};
				auto value = group == 0   ? info->mTransform.GetLocalPosition()
							 : group == 1 ? Math::degrees(info->mTransform.GetLocalRotationEular())
										  : info->mTransform.GetLocalScale();
				vector(transform, names[group], value, [entity, group](int axis, float number) {
					auto* info = Component<Components::EntityInfoComponent>(entity);
					if (!info)
						return;
					auto value = group == 0   ? info->mTransform.GetLocalPosition()
								 : group == 1 ? Math::degrees(info->mTransform.GetLocalRotationEular())
											  : info->mTransform.GetLocalScale();
					value[axis] = number;
					if (group == 0)
						info->mTransform.SetPosition(value);
					else if (group == 1)
						info->mTransform.SetRotation(Math::radians(value));
					else
						info->mTransform.SetScale(value);
				});
			}
		}
		if (auto* sky = Component<SkyboxComponent>(entity))
		{
			auto& section = componentSection.operator()<SkyboxComponent>("SKYBOX");
			Label(section, "Default sky background", 13);
			toggle(section, "Visible", sky->Enabled, [entity](bool value) {
				if (auto* sky = Component<SkyboxComponent>(entity))
					sky->Enabled = value;
			});
		}
		if (auto* light = Component<Components::LightComponent>(entity))
		{
			auto& section = componentSection.operator()<Components::LightComponent>("LIGHT");

			// Segmented radio button group for selecting the active light type
			const auto currentType = light->GetType();
			size_t selectedTypeIndex = 0;
			if (currentType == Components::LightComponent::Type::Point)
				selectedTypeIndex = 1;
			else if (currentType == Components::LightComponent::Type::Spot)
				selectedTypeIndex = 2;
			else
				selectedTypeIndex = 0; // Directional or Unknown

			SegmentedButton(section, "Type", {"Directional", "Point", "Spot"}, selectedTypeIndex,
				[entity, changed, &history](size_t newIndex) {
					Components::LightComponent::Type newType = Components::LightComponent::Type::Directional;
					if (newIndex == 1)
						newType = Components::LightComponent::Type::Point;
					else if (newIndex == 2)
						newType = Components::LightComponent::Type::Spot;

					if (auto* light = Component<Components::LightComponent>(entity))
					{
						if (light->GetType() != newType)
						{
							history.Edit(entity, "Change light type", [entity, newType]() {
								if (auto* light = Component<Components::LightComponent>(entity))
									light->SetType(newType);
							});
							changed(true);
						}
					}
				});

			number(
				section, "Intensity", light->GetIntensity(),
				[entity](float value) {
					if (auto* light = Component<Components::LightComponent>(entity))
						light->SetIntensity(value);
				},
				0, 100000);

			if (currentType == Components::LightComponent::Type::Directional ||
				currentType == Components::LightComponent::Type::Spot ||
				currentType == Components::LightComponent::Type::Unknown)
			{
				vector(section, "Direction", light->GetDirection(), [entity](int axis, float number) {
					if (auto* light = Component<Components::LightComponent>(entity))
					{
						auto dir = light->GetDirection();
						dir[axis] = number;
						light->SetDirection(dir);
					}
				}, -100000.0f, 100000.0f);
			}

			auto color = light->GetColor();
			section.Add<EditorColorProperty>(colors, "Light color", Math::Vector3(color.r, color.g, color.b),
				[entity, changed, &history](Math::Vector3 value) {
					history.Edit(entity, "Light color", [&]() {
						if (auto* light = Component<Components::LightComponent>(entity))
							light->SetColor(Graphics::Color(value.r, value.g, value.b, 1.f));
					});
					changed(false);
				}, 100000.f);

			if (currentType == Components::LightComponent::Type::Point ||
				currentType == Components::LightComponent::Type::Spot)
			{
				vector(section, "Attenuation", light->GetAttenuation(), [entity](int axis, float number) {
					if (auto* light = Component<Components::LightComponent>(entity))
					{
						auto att = light->GetAttenuation();
						att[axis] = number;
						light->SetAttenuation(att);
					}
				}, 0.0f, 100000.0f);
			}

			if (currentType == Components::LightComponent::Type::Spot)
			{
				auto cone = light->GetSpotlightCone();
				for (int axis = 0; axis < 2; ++axis)
					number(
						section, axis == 0 ? "Inner cone (cosine)" : "Outer cone (cosine)", cone[axis],
						[entity, axis](float number) {
							if (auto* light = Component<Components::LightComponent>(entity))
							{
								auto cone = light->GetSpotlightCone();
								cone[axis] = number;
								light->SetSpotlightCone(cone);
							}
						},
						-1, 1);
			}

			if (currentType == Components::LightComponent::Type::Point ||
				currentType == Components::LightComponent::Type::Spot)
			{
				number(
					section, "Near plane", light->GetNearPlane(),
					[entity](float value) {
						if (auto* light = Component<Components::LightComponent>(entity))
							light->SetNearPlane(value);
					},
					.001f);
				number(
					section, "Far plane", light->GetFarPlane(),
					[entity](float value) {
						if (auto* light = Component<Components::LightComponent>(entity))
							light->SetFarPlane(value);
					},
					.001f);
			}

			if (currentType == Components::LightComponent::Type::Spot)
			{
				number(
					section, "Field of view (degrees)", light->GetFOV(),
					[entity](float value) {
						if (auto* light = Component<Components::LightComponent>(entity))
							light->SetFOV(value);
					},
					1, 179);
			}

			toggle(section, "Cast shadows", light->mCastShadows, [entity](bool value) {
				if (auto* light = Component<Components::LightComponent>(entity))
					light->mCastShadows = value;
			});
		}
		if (auto* mesh = Component<Components::MeshComponent>(entity))
		{
			auto& section = componentSection.operator()<Components::MeshComponent>("MESH");
			section.Add<EditorAssetProperty>(Assets::AssetType::Mesh, mesh->GetMesh(), mAssetPreviews,
				[entity, changed, &history](Assets::IAsset* asset) {
					auto* mesh = Component<Components::MeshComponent>(entity);
					if (!mesh || mesh->GetMesh() == asset) return;
					history.Edit(entity, "Assign mesh", [entity, asset]() {
						if (auto* mesh = Component<Components::MeshComponent>(entity))
							mesh->SetMesh(static_cast<Assets::Mesh*>(asset));
					});
					changed(true);
				});
			section.Add<EditorAssetProperty>(Assets::AssetType::Material, mesh->GetMaterial(), mAssetPreviews,
				[entity, changed, &history](Assets::IAsset* asset) {
					auto* mesh = Component<Components::MeshComponent>(entity);
					if (!mesh || mesh->GetMaterial() == asset) return;
					history.Edit(entity, "Assign material", [entity, asset]() {
						if (auto* mesh = Component<Components::MeshComponent>(entity))
							mesh->SetMaterial(static_cast<Assets::Material*>(asset));
					});
					changed(true);
				});
			toggle(section, "Visible", mesh->GetEnableRendering(), [entity](bool value) {
				if (auto* mesh = Component<Components::MeshComponent>(entity))
					mesh->SetEnableRendering(value);
			});
			toggle(section, "Cast shadows", mesh->GetCastShadow(), [entity](bool value) {
				if (auto* mesh = Component<Components::MeshComponent>(entity))
					mesh->SetCastShadow(value);
			});
			toggle(section, "Receive shadows", mesh->GetReceiveShadows(), [entity](bool value) {
				if (auto* mesh = Component<Components::MeshComponent>(entity))
					mesh->SetReceiveShadows(value);
			});
		}
		if (auto* body = Component<Components::RigidBodyComponent>(entity))
		{
			auto& section = componentSection.operator()<Components::RigidBodyComponent>("RIGID BODY");
			toggle(section, "Kinematic (authoring)", body->isKinematic, [entity](bool value) {
				if (auto* body = Component<Components::RigidBodyComponent>(entity))
					body->isKinematic = value;
			});
		}
		if (Component<Components::ColliderComponent>(entity))
		{
			auto& section = componentSection.operator()<Components::ColliderComponent>("COLLIDER");
			Label(section, "Collider / authoring data", 13);
		}
		if (Component<Components::CameraComponent>(entity))
		{
			auto& section = componentSection.operator()<Components::CameraComponent>("CAMERA");
			Label(section, "Camera / scene component", 13);
		}
		if (Component<Components::ScriptComponent>(entity))
		{
			auto& section = componentSection.operator()<Components::ScriptComponent>("SCRIPT");
			Label(section, "Script / runtime execution disabled", 13);
		}
		if (Component<Components::AudioListenerComponent>(entity))
		{
			auto& section = componentSection.operator()<Components::AudioListenerComponent>("AUDIO LISTENER");
			Label(section, "Audio listener / authoring data", 13);
		}
		if (Component<Components::AudioSourceComponent>(entity))
		{
			auto& section = componentSection.operator()<Components::AudioSourceComponent>("AUDIO SOURCE");
			Label(section, "Audio source / authoring data", 13);
		}
		AddComponent<Components::LightComponent>(add, entity, "Light", changed, history);
		AddComponent<Components::MeshComponent>(add, entity, "Mesh", changed, history);
		// Runtime service objects are not activated in the editor's authoring registry.
		AddComponent<Components::RigidBodyComponent>(add, entity, "Rigid body", changed, history);
		AddComponent<Components::ColliderComponent>(add, entity, "Collider", changed, history);
		AddComponent<Components::ScriptComponent>(add, entity, "Script", changed, history);
		AddComponent<Components::AudioListenerComponent>(add, entity, "Audio listener", changed, history);
		AddComponent<Components::AudioSourceComponent>(add, entity, "Audio source", changed, history);
		if (!Component<Components::CameraComponent>(entity))
			Action(add, "Camera", [entity, changed, &history]() {
				auto& registry = Core::Scene::Get().GetRegistry();
				if (!registry.valid(entity) || registry.all_of<Components::CameraComponent>(entity))
					return;
				Components::CameraComponentDesc desc;
				desc.mInitRTs = false;
				desc.mIsActiveCamera = false;
				desc.mProjection = Math::perspective(Math::radians(45.0f), 1.0f, .1f, 100.f);
				history.Edit(entity, "Add camera", [&]() {
					registry.emplace<Components::CameraComponent>(entity, desc);
				});
				changed(true);
			});
		if (Core::Scene::Get().GetRegistry().all_of<
				Components::LightComponent, Components::MeshComponent, Components::RigidBodyComponent,
				Components::ColliderComponent, Components::ScriptComponent,
				Components::AudioListenerComponent, Components::AudioSourceComponent,
				Components::CameraComponent>(entity))
			Label(add, "All components added", 13);
		return root;
	}
} // namespace Nuclear::Editor
