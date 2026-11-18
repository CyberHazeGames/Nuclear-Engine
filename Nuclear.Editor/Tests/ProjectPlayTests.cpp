#include "ProjectPlayTests.h"
#include <EditorUI.h>
#include <SkyboxComponent.h>
#include <Animation/Animator.h>
#include <Assets/Animations.h>
#include <ProjectPlayer.h>
#include <fstream>
#include <iostream>
#include <thread>
#include <Windows.h>

namespace Nuclear::Editor::Tests
{
	namespace
	{
		void Check(bool value, const char* message)
		{
			if (!value) throw std::runtime_error(message);
		}
		entt::entity FindEntity(const std::string& name)
		{
			auto view = Core::Scene::Get().GetRegistry().view<Components::EntityInfoComponent>();
			for (auto id : view)
				if (view.get<Components::EntityInfoComponent>(id).mName == name) return id;
			return entt::null;
		}
		bool HasLabel(const std::string& text)
		{
			UI::Widget marker;
			auto& context = UI::UIModule::Get().GetContext();
			for (UI::WidgetId id = 1; id < marker.GetId(); ++id)
				if (auto* label = dynamic_cast<UI::TextBlock*>(context.Find(id)); label && label->GetText().find(text) != std::string::npos)
					return true;
			return false;
		}
	}
	void RunProjectPlayTests()
	{
		const auto folder = std::filesystem::absolute("Build/DebugX64") / Core::UUID::CreateNewUUID().str();
		std::filesystem::create_directories(folder);
		const auto saved = folder / "test game.nuclearproject";
		auto& scene = Core::Scene::Get();
		auto& registry = scene.GetRegistry();
		{
			Project project;
			EditorUI editor(project);
			editor.Initialize();
			auto skyboxes = registry.view<SkyboxComponent>();
			Check(skyboxes.size() == 1 && skyboxes.get<SkyboxComponent>(skyboxes.front()).Enabled,
				"First launch must create one enabled skybox");
			const auto firstProject = folder / "Fresh.nuclearproject";
			editor.SaveProject(firstProject);
			const auto startup = ProjectDocument::Read(firstProject).StartupMap;
			Check(startup.extension() == ".NMap", "Fresh project must save a startup NMap");
			editor.OpenProject(firstProject);
			Check(skyboxes.size() == 1 && skyboxes.get<SkyboxComponent>(skyboxes.front()).Enabled,
				"Reload must preserve the saved skybox exactly once");
			skyboxes.get<SkyboxComponent>(skyboxes.front()).Enabled = false;
			editor.SaveMap(startup);
			editor.OpenMap(startup);
			Check(skyboxes.size() == 1 && !skyboxes.get<SkyboxComponent>(skyboxes.front()).Enabled,
				"Opening a map must preserve a disabled skybox");
			editor.OpenProject("../Samples/Pickups/Pickups.nuclearproject");
			editor.Update(.016f);
			editor.Render(.016f);
			auto extra = scene.CreateEntity("Falling cube", ECS::Transform({3, 4, 0}));
			extra.AddComponent<Components::ColliderComponent>();
			extra.AddComponent<Components::RigidBodyComponent>();
			const auto child = scene.CreateEntity("Child", ECS::Transform({1, 0, 0})).GetEntityID();
            auto& parentInfo = registry.get<Components::EntityInfoComponent>(extra.GetEntityID());
            Check(scene.SetParent(child, parentInfo.PersistentId), "Parent assignment failed");
            Check(!scene.SetParent(extra.GetEntityID(), registry.get<Components::EntityInfoComponent>(child).PersistentId), "Accepted cycle");
            Check(registry.get<Components::EntityInfoComponent>(child).mTransform.GetWorldPosition().x == 4, "Wrong world transform");
            const auto model = folder / "triangle.obj";
			std::ofstream(model) << "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
			auto* mesh = project.ImportMesh(model);
			Check(mesh && !mesh->GetImportedAnimations(), "Static model retained an empty animation asset");
			extra.AddComponent<Components::MeshComponent>(mesh, project.GetMeshMaterial(*mesh)).Tint = {.2f, .3f, .4f};
            auto* animated = project.ImportMesh("Tests/Fixtures/TwoClips.gltf");
            auto* clips = animated->GetImportedAnimations();
            Check(clips && clips->mClips.size() == 2, "Importer lost animation clips");
            auto dancer = scene.CreateEntity("Animated triangle");
            auto& component = dancer.AddComponent<Components::MeshComponent>(animated, project.GetMeshMaterial(*animated));
            Check(component.PlayClip(0, 0), "Could not start animation");
            component.AdvanceClip(.01f);
            Check(component.PlayClip(1, 1), "Could not crossfade");
            component.AdvanceClip(.5f);
            auto pose = component.GetAnimator()->GetFinalBoneMatrices().front();
            Check(std::abs(pose[3].x - 1.f) < .01f, "Animation crossfade did not interpolate the joint");
			editor.SaveProject(saved);
			editor.SaveProject(saved);
			auto invalid = folder / "invalid.nuclearproject";
			std::ofstream(invalid) << "NuclearProject 1\nproject \"broken\"";
			bool failed = false;
			try { editor.OpenProject(invalid); } catch (const std::exception&) { failed = true; }
			Check(failed && registry.valid(extra.GetEntityID()), "Invalid project replaced the authoring scene");
			editor.Shutdown();
		}
		{
			Project freshProject;
			EditorUI fresh(freshProject);
			fresh.Initialize();
			fresh.OpenProject(saved);
			fresh.Update(.016f);
			fresh.Render(.016f);
			auto player = FindEntity("Player");
			const auto pickup = FindEntity("Pickup 1");
			Check(player != entt::null && pickup != entt::null, "Saved player or pickups missing");
			Check(registry.get<Components::ScriptComponent>(player).ClassName == "Samples.PickupPlayer", "Lost script assignment");
			auto* restored = registry.get<Components::MeshComponent>(FindEntity("Falling cube")).GetMesh();
			scene.UpdateTransforms();
            auto& childInfo = registry.get<Components::EntityInfoComponent>(FindEntity("Child"));
            auto& parentInfo = registry.get<Components::EntityInfoComponent>(FindEntity("Falling cube"));
            Check(childInfo.ParentId == parentInfo.PersistentId && childInfo.mTransform.GetWorldPosition().x == 4, "Lost parent on reopen");
            auto& tint = registry.get<Components::MeshComponent>(FindEntity("Falling cube")).Tint;
            Check(tint == Math::Vector3(.2f, .3f, .4f), "Lost surface override on reopen");
            Check(registry.get<Components::MeshComponent>(FindEntity("Animated triangle")).ClipIndex == 1, "Lost authored clip");
            Check(restored && restored->GetState() == Assets::IAsset::State::Created, "Saved model did not import");
			fresh.Play();
			// The game sets this title only after assets, scripts and physics have finished loading.
			bool rendering = false;
			auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(35);
			while (std::chrono::steady_clock::now() < deadline && !rendering)
			{
				fresh.Update(.016f);
				rendering = FindWindowW(L"Nuclear.Platform.Window", L"Three Pickups - Play") != nullptr;
				if (HasLabel("Play failed")) throw std::runtime_error("Child failed to start");
				std::this_thread::sleep_for(std::chrono::milliseconds(40));
			}
			Check(rendering, "Child did not load its isolated scene");
			fresh.Stop();
			deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
			while (!HasLabel("Play stopped") && std::chrono::steady_clock::now() < deadline)
			{
				fresh.Update(.016f);
				std::this_thread::sleep_for(std::chrono::milliseconds(25));
			}
			Check(HasLabel("Play stopped") && registry.valid(pickup), "Stop failed to restore authoring state");
			fresh.Shutdown();
		}
		// Editing a linked map must keep the main project's startup map and project-wide settings.
		{
			Project project;
			EditorUI editor(project);
			editor.Initialize();
			editor.OpenProject("../Samples/TwinRooms/TwinRooms.nuclearproject");
			const auto main = folder / "rooms.nuclearproject";
			editor.SaveProject(main);
			const auto startup = ProjectDocument::Read(main).StartupMap;
			editor.OpenMap("../Samples/TwinRooms/Gallery.NMap");
			const auto room = folder / "copy.NMap";
			editor.SaveMap(room);
			// Reopen a separate, linked copy. Use its registered name in the manifest.
			auto manifest = ProjectDocument::Read(main);
			manifest.Levels["Gallery"] = room;
			manifest.SaveManifest(main);
			editor.OpenProject(main);
			editor.OpenMap(room);
			editor.SaveProject(main);
			auto reopened = ProjectDocument::Read(main);
			Check(reopened.StartupMap == startup && reopened.SceneName == "Title"
				&& reopened.Levels.at("Gallery") == room && reopened.Prefabs.contains("Coin"),
				"Saving a linked map changed the main project");
			editor.Shutdown();
			std::filesystem::remove(room);
			std::filesystem::remove(startup);
			std::filesystem::remove(main);
		}
		// A legacy project with an embedded startup and a linked project-format level upgrades on save.
		{
			const auto legacy = folder / "legacy.nuclearproject";
			const auto linked = folder / "old level.nuclearproject";
			const std::string scene = " 0 0 0 1 -1\nentities 0\nend\n";
			std::ofstream(linked) << "NuclearProject 1\nproject \"Legacy\"\nassembly \"\"\nstartup \"Other\"" << scene;
			std::ofstream(legacy) << "NuclearProject 2\nproject \"Legacy\"\nassembly \"\"\nlevels 1\n"
				"level \"Other\" \"old level.nuclearproject\"\nprefabs 0\nstartup \"First\"" << scene;
			Project project;
			EditorUI editor(project);
			editor.Initialize();
			editor.OpenProject(legacy);
			editor.SaveProject(legacy);
			auto migrated = ProjectDocument::Read(legacy);
			const auto other = migrated.Levels.at("Other");
			Check(migrated.StartupMap.extension() == ".NMap" && other.extension() == ".NMap"
				&& ProjectDocument::Read(other).SceneName == "Other" && std::filesystem::exists(linked),
				"Legacy map migration failed");
			editor.Shutdown();
			for (const auto& file : {migrated.StartupMap, other, linked, legacy}) std::filesystem::remove(file);
		}
		// Execute two fresh runs and inspect the real C# callbacks, HUD and physics.
		for (int run = 0; run < 2; ++run)
		{
			ProjectPlayer game(ProjectDocument::Read(saved));
			game.Load();
			Check(HasLabel("0/3"), "Managed OnStart did not reset score");
			auto window = static_cast<HWND>(Core::Engine::Get().GetMainWindow()->GetNativeHandle());
			const auto player = FindEntity("Player");
			const auto start = registry.get<Components::EntityInfoComponent>(player).mTransform.GetLocalPosition();
			SendMessageW(window, WM_KEYDOWN, 'W', 0x00110001);
			for (int i = 1; i <= 3; ++i)
			{
				const auto pickup = FindEntity("Pickup " + std::to_string(i));
				Check(pickup != entt::null, "Pickup was not restored");
				for (int frame = 0; frame < 70 && registry.valid(pickup); ++frame)
					game.Update(1.f / 60.f);
				game.Render(1.f / 60.f);
				Check(!registry.valid(pickup) && HasLabel(std::to_string(i) + "/3"), "Pickup removal or score failed");
			}
			SendMessageW(window, WM_KEYUP, 'W', 0xC0110001);
			Check(scene.GetMainCamera()->GetPosition().z < start.z - 7.f, "Camera did not follow C# movement");
			auto& body = registry.get<Components::EntityInfoComponent>(FindEntity("Falling cube")).mTransform;
			Check(body.GetLocalPosition().y < 4.f, "Physics did not advance");
			UI::UIModule::Get().GetContext().Clear();
			game.Shutdown();
		}
		for (const auto* name : {"Fresh.nuclearproject", "Fresh.NMap", "test game.nuclearproject", "test game.NMap", "invalid.nuclearproject", "triangle.obj"})
			std::filesystem::remove(folder / name);
		std::filesystem::remove(folder);
		std::cout << "Project round trip, isolated game window, Play/Stop, two managed pickup runs and physics passed.\n";
	}
}
