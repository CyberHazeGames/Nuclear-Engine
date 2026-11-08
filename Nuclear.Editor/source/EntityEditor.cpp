#include <EntityEditor.h>
#include <EditorWidgets.h>
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
						  const std::function<void(bool)>& changed)
		{
			if (Component<T>(entity))
				return;
			Action(parent, title, [entity, changed]() {
				auto& registry = Core::Scene::Get().GetRegistry();
				if (registry.valid(entity) && !registry.all_of<T>(entity))
					registry.emplace<T>(entity);
				changed(true);
			});
		}
	} // namespace
	std::unique_ptr<UI::Widget> EntityEditor::Build(entt::entity entity, std::function<void(bool)> changed)
	{
		auto root = std::make_unique<UI::VerticalBox>();
		root->SetSpacing(12);
		auto* info = Component<Components::EntityInfoComponent>(entity);
		if (!info)
		{
			Label(*root, "Select an entity in the hierarchy.").SetWrap(true);
			return root;
		}
		Label(*root, "ENTITY / " + std::to_string(static_cast<Uint32>(entity)), 13);
		auto& name = root->Add<UI::TextBox>(info->mName);
		name.OnCommitted = [entity, changed](const std::string& value) {
			if (auto* info = Component<Components::EntityInfoComponent>(entity))
				info->mName = value.empty() ? "Entity" : value;
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
		auto& transform = Section(*root, "TRANSFORM");
		Label(transform, "Enter commits a value. Rotation is in degrees.", 12).SetWrap(true);
		for (int group = 0; group < 3; ++group)
		{
			const char* names[] = {"POSITION", "ROTATION", "SCALE"};
			Label(transform, names[group], 12);
			auto& row = transform.Add<UI::HorizontalBox>();
			row.SetSpacing(6);
			auto value = group == 0	  ? info->mTransform.GetLocalPosition()
						 : group == 1 ? Math::degrees(info->mTransform.GetLocalRotationEular())
									  : info->mTransform.GetLocalScale();
			for (int axis = 0; axis < 3; ++axis)
			{
				auto& column = row.Add<UI::VerticalBox>();
				Fill(row, column);
				Number(column, std::string(1, "XYZ"[axis]), value[axis], [entity, group, axis](float number) {
					auto* info = Component<Components::EntityInfoComponent>(entity);
					if (!info)
						return;
					auto value = group == 0	  ? info->mTransform.GetLocalPosition()
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
		if (auto* light = Component<Components::LightComponent>(entity))
		{
			auto& section = Section(*root, "LIGHT");
			Number(
				section, "Intensity", light->GetIntensity(),
				[entity](float value) {
					if (auto* light = Component<Components::LightComponent>(entity))
						light->SetIntensity(value);
				},
				0, 100000);
			auto vector = [&section, entity](const std::string& title, Math::Vector3 value, int property) {
				Label(section, title, 13);
				for (int axis = 0; axis < 3; ++axis)
					Number(
						section, std::string(1, "XYZ"[axis]), value[axis],
						[entity, property, axis](float number) {
							auto* light = Component<Components::LightComponent>(entity);
							if (!light)
								return;
							auto current = property == 0 ? light->GetDirection()
										   : property == 1
											   ? Math::Vector3(light->GetColor().r, light->GetColor().g,
															   light->GetColor().b)
											   : light->GetAttenuation();
							current[axis] = number;
							if (property == 0)
								light->SetDirection(current);
							else if (property == 1)
								light->SetColor(Graphics::Color(current.x, current.y, current.z, 1.0f));
							else
								light->SetAttenuation(current);
						},
						property == 0 ? -100000.0f : 0.0f);
			};
			vector("Direction", light->GetDirection(), 0);
			auto color = light->GetColor();
			vector("Color RGB", {color.r, color.g, color.b}, 1);
			vector("Attenuation", light->GetAttenuation(), 2);
			auto cone = light->GetSpotlightCone();
			for (int axis = 0; axis < 2; ++axis)
				Number(
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
			Number(
				section, "Near plane", light->GetNearPlane(),
				[entity](float value) {
					if (auto* light = Component<Components::LightComponent>(entity))
						light->SetNearPlane(value);
				},
				.001f);
			Number(
				section, "Far plane", light->GetFarPlane(),
				[entity](float value) {
					if (auto* light = Component<Components::LightComponent>(entity))
						light->SetFarPlane(value);
				},
				.001f);
			Number(
				section, "Field of view (degrees)", light->GetFOV(),
				[entity](float value) {
					if (auto* light = Component<Components::LightComponent>(entity))
						light->SetFOV(value);
				},
				1, 179);
			Toggle(section, "Cast shadows", light->mCastShadows, [entity](bool value) {
				if (auto* light = Component<Components::LightComponent>(entity))
					light->mCastShadows = value;
			});
		}
		if (auto* mesh = Component<Components::MeshComponent>(entity))
		{
			auto& section = Section(*root, "MESH");
			Label(section, mesh->GetMesh() ? mesh->GetMesh()->GetName() : "No mesh assigned").SetWrap(true);
			Label(section, mesh->GetMaterial() ? mesh->GetMaterial()->GetName() : "No material assigned")
				.SetWrap(true);
			Toggle(section, "Visible", mesh->GetEnableRendering(), [entity](bool value) {
				if (auto* mesh = Component<Components::MeshComponent>(entity))
					mesh->SetEnableRendering(value);
			});
			Toggle(section, "Cast shadows", mesh->GetCastShadow(), [entity](bool value) {
				if (auto* mesh = Component<Components::MeshComponent>(entity))
					mesh->SetCastShadow(value);
			});
			Toggle(section, "Receive shadows", mesh->GetReceiveShadows(), [entity](bool value) {
				if (auto* mesh = Component<Components::MeshComponent>(entity))
					mesh->SetReceiveShadows(value);
			});
		}
		if (auto* body = Component<Components::RigidBodyComponent>(entity))
			Toggle(*root, "Kinematic (authoring)", body->isKinematic, [entity](bool value) {
				if (auto* body = Component<Components::RigidBodyComponent>(entity))
					body->isKinematic = value;
			});
		if (Component<Components::ColliderComponent>(entity))
			Label(*root, "Collider / authoring data", 13);
		if (Component<Components::CameraComponent>(entity))
			Label(*root, "Camera / scene component", 13);
		if (Component<Components::ScriptComponent>(entity))
			Label(*root, "Script / runtime execution disabled", 13);
		if (Component<Components::AudioListenerComponent>(entity))
			Label(*root, "Audio listener / authoring data", 13);
		if (Component<Components::AudioSourceComponent>(entity))
			Label(*root, "Audio source / authoring data", 13);
		AddComponent<Components::LightComponent>(add, entity, "Light", changed);
		AddComponent<Components::MeshComponent>(add, entity, "Mesh", changed);
		// Runtime service objects are not activated in the editor's authoring registry.
		AddComponent<Components::RigidBodyComponent>(add, entity, "Rigid body", changed);
		AddComponent<Components::ColliderComponent>(add, entity, "Collider", changed);
		AddComponent<Components::ScriptComponent>(add, entity, "Script", changed);
		AddComponent<Components::AudioListenerComponent>(add, entity, "Audio listener", changed);
		AddComponent<Components::AudioSourceComponent>(add, entity, "Audio source", changed);
		if (!Component<Components::CameraComponent>(entity))
			Action(add, "Camera", [entity, changed]() {
				auto& registry = Core::Scene::Get().GetRegistry();
				if (!registry.valid(entity) || registry.all_of<Components::CameraComponent>(entity))
					return;
				Components::CameraComponentDesc desc;
				desc.mInitRTs = false;
				desc.mIsActiveCamera = false;
				desc.mProjection = Math::perspective(Math::radians(45.0f), 1.0f, .1f, 100.f);
				registry.emplace<Components::CameraComponent>(entity, desc);
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
