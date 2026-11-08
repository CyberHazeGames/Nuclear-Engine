#include "EditorHistoryTests.h"
#include <EditorHistory.h>
#include <stdexcept>

namespace Nuclear::Editor::Tests
{
	namespace
	{
		void Check(bool condition, const char* message)
		{
			if (!condition)
				throw std::runtime_error(message);
		}
	}

	void RunEditorHistoryTests()
	{
		auto& scene = Core::Scene::Get();
		auto& registry = scene.GetRegistry();
		EditorHistory history;
		Check(!history.Undo() && !history.Redo(), "Empty history is safe");
		const auto entity = scene.CreateEntity("History fixture").GetEntityID();
		history.RecordCreated(entity);
		history.Edit(entity, "Rename", [&]() {
			registry.get<Components::EntityInfoComponent>(entity).mName = "Renamed fixture";
		});
		history.Edit(entity, "Transform", [&]() {
			auto& transform = registry.get<Components::EntityInfoComponent>(entity).mTransform;
			transform.SetPosition({2, -3, 4});
			transform.SetRotation(Math::radians(Math::Vector3(20, 30, 40)));
			transform.SetScale({-2, 3, 4});
		});
		history.Edit(entity, "Components", [&]() {
			auto& light = registry.emplace<Components::LightComponent>(entity);
			light.SetIntensity(7);
			light.SetDirection({1, -2, 3});
			light.SetColor(Graphics::Color(.2f, .4f, .8f, 1.f));
			light.SetNearPlane(.2f);
			light.SetFarPlane(90);
			light.SetFOV(70);
			light.SetSpotlightCone({.9f, .7f});
			light.SetAttenuation({1, .1f, .01f});
			light.mCastShadows = true;
			registry.emplace<Components::MeshComponent>(entity).SetEnableRendering(false);
			registry.emplace<Components::RigidBodyComponent>(entity).isKinematic = true;
			registry.emplace<Components::ColliderComponent>(entity);
			registry.emplace<Components::ScriptComponent>(entity);
			registry.emplace<Components::AudioListenerComponent>(entity);
			registry.emplace<Components::AudioSourceComponent>(entity);
			Components::CameraComponentDesc desc;
			desc.mInitRTs = false;
			desc.mIsActiveCamera = false;
			desc.mProjection = Math::Matrix4(1);
			registry.emplace<Components::CameraComponent>(entity, desc);
		});
		const auto authored = EditorEntityState::Capture(entity);
		history.Edit(entity, "Delete", [&]() { registry.destroy(entity); });
		// A later creation reuses the slot with a new version. Undo must unwind both identities.
		const auto later = scene.CreateEntity("Later fixture").GetEntityID();
		history.RecordCreated(later);
		Check(history.Undo() && !registry.valid(later), "Undo removes the later version");
		Check(history.Undo() && registry.valid(entity), "Undo deletion restores the original identity");
		Check(EditorEntityState::Capture(entity) == authored, "Deletion restores all editable authoring data");
		Check(!registry.get<Components::CameraComponent>(entity).GetIsActive() &&
			!registry.get<Components::CameraComponent>(entity).GetColorRT().GetSRV() &&
			registry.get<Components::AudioSourceComponent>(entity).GetSourceID() == 0,
			"Restoring components does not activate runtime services");
		Check(history.Undo() && !registry.any_of<Components::LightComponent, Components::MeshComponent,
			Components::CameraComponent, Components::RigidBodyComponent, Components::ColliderComponent,
			Components::ScriptComponent, Components::AudioListenerComponent, Components::AudioSourceComponent>(entity),
			"Undo component addition removes all added components");
		Check(history.Redo() && EditorEntityState::Capture(entity) == authored, "Redo restores component values");
		Check(history.Undo() && history.Undo(), "Earlier edits remain reachable after deletion restoration");
		Check(registry.get<Components::EntityInfoComponent>(entity).mTransform.GetLocalPosition() == Math::Vector3(0),
			"Transform undo restores position");
		Check(history.Undo() && registry.get<Components::EntityInfoComponent>(entity).mName == "History fixture",
			"Rename undo survives entity deletion and recreation");
		history.Edit(entity, "No change", []() {});
		Check(history.CanRedo(), "No-op edits preserve redo");
		history.Edit(entity, "Branch", [&]() { registry.get<Components::EntityInfoComponent>(entity).mName = "Branch"; });
		Check(!history.CanRedo(), "A new committed edit clears redo");
		Check(history.Undo() && history.Undo() && !registry.valid(entity), "Undo creation removes restored entity");
		Check(history.Redo() && registry.valid(entity), "Redo creation preserves its original handle");
		history.Clear();
		Check(!history.CanUndo() && !history.CanRedo(), "Clearing history resets both directions");
		for (int index = 0; index < 140; ++index)
			history.Edit(entity, "Rename", [&]() {
				registry.get<Components::EntityInfoComponent>(entity).mName = std::to_string(index);
			});
		int undone = 0;
		while (history.Undo())
			++undone;
		Check(undone == 128 && registry.valid(entity), "Bounded history retains the latest 128 actions");
		history.Clear();
		registry.destroy(entity);
	}
}
