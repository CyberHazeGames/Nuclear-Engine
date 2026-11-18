#include <ProjectPlayer.h>
#include <SkyboxComponent.h>
#include <Rendering/RenderingModule.h>
#include <Scripting/ScriptingModule.h>
#include <Assets/DefaultMeshes.h>
#include <algorithm>
#include <Nuclear.Platform/Platform.h>
#include <Utilities/Logger.h>

namespace Nuclear::Editor
{
	int ProjectPlayer::RunGame(const std::filesystem::path& file, const std::filesystem::path& assets)
	{
		if (!ProjectDocument::IsProject(file)) throw std::runtime_error("Choose a main .nuclearproject to play");
		auto doc = Nuclear::Editor::ProjectDocument::Read(file);
        std::map<std::string, ProjectDocument> levels;
        for (const auto& [name, path] : doc.Levels)
        {
            if (name == doc.SceneName) throw std::runtime_error("A linked level uses the startup scene name");
            auto level = ProjectDocument::Read(path);
            if (!level.Assembly.empty() && level.Assembly != doc.Assembly)
                throw std::runtime_error("Linked levels must share the startup gameplay assembly");
            level.SceneName = name;
            level.ProjectName = doc.ProjectName;
            level.Prefabs.insert(doc.Prefabs.begin(), doc.Prefabs.end());
            levels.emplace(name, std::move(level));
        }
        auto current = doc.SceneName;
        levels.emplace(current, doc);
        Core::EngineStartupDesc desc;
		desc.mEngineWindowDesc.Title = doc.ProjectName + " - Loading";
		desc.mAssetsLibraryPath = std::filesystem::absolute(assets).generic_string() + "/";
		desc.AutoInitAudioModule = doc.NeedsAudio();
		desc.AutoInitPhysXModule = doc.NeedsPhysics();
		desc.AutoInitScriptingModule = doc.NeedsScripts();
        for (auto& [key, path] : doc.StartupPrefabs) levels.at(current).Prefabs[key] = path;
        for (const auto& [name, level] : levels)
        {
            desc.AutoInitAudioModule |= level.NeedsAudio();
            desc.AutoInitPhysXModule |= level.NeedsPhysics();
            desc.AutoInitScriptingModule |= level.NeedsScripts();
            for (const auto& [key, path] : level.Prefabs)
            {
                const auto prefab = ProjectDocument::Read(path);
                desc.AutoInitAudioModule |= prefab.NeedsAudio();
                desc.AutoInitPhysXModule |= prefab.NeedsPhysics();
                desc.AutoInitScriptingModule |= prefab.NeedsScripts();
            }
        }
		desc.AutoInitThreadingModule = false;
		desc.EnableImGui = false;
		desc.mScriptingRuntimeDirectory = Platform::GetExecutablePath().parent_path();
		desc.mScriptingClientDllName = (doc.Assembly.empty() ? desc.mScriptingRuntimeDirectory / "SamplesScripts.dll" : doc.Assembly).string();
		auto& engine = Core::Engine::Get();
		if (!engine.Start(desc)) throw std::runtime_error("Game engine startup failed. Check runtime dependencies.");
        while (!current.empty())
        {
            auto scene = levels.at(current);
            scene.Levels = doc.Levels;
            scene.Levels.emplace(doc.SceneName, file);
            ProjectPlayer player(std::move(scene));
            try { engine.LoadClient(&player); }
            catch (...) { engine.EndClient(); engine.Shutdown(); throw; }
            engine.EndClient();
            current = player.mNextLevel;
        }
		engine.Shutdown();
		return 0;
	}

	ProjectPlayer::ProjectPlayer(ProjectDocument document) : mDocument(std::move(document))
	{
		for (const auto& [name, path] : mDocument.StartupPrefabs) mDocument.Prefabs[name] = path;
	}

	void ProjectPlayer::Load()
	{
		auto& scene = GetScene();
		mProject.InitializeMaterials();
		mProject.AddNewScene()->SetName(mDocument.SceneName);
		mDocument.Resolve(mProject);
        for (const auto& [name, path] : mDocument.Prefabs)
        {
            auto doc = ProjectDocument::Read(path);
            doc.Resolve(mProject);
            mTemplates.emplace(name, doc.Template());
        }
		mCamera = mDocument.Instantiate();
		scene.UpdateTransforms();
		auto& reg = scene.GetRegistry();
		if (!reg.valid(mCamera)) throw std::runtime_error("Select a game camera before Play");
		auto& camera = reg.get<Components::CameraComponent>(mCamera);
		auto& rendering = Rendering::RenderingModule::Get();
		auto colorTarget = rendering.GetFinalRT().GetDesc();
		colorTarget.ColorTexFormat = Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB;
		camera.CreateRenderTargets(colorTarget, rendering.GetFinalDepthRT().GetDesc());
		camera.SetIsActive(true);
		const auto color = mDocument.ClearColor;
		camera.mRTClearColor = Graphics::Color(color.r, color.g, color.b, color.a);
		scene.SetMainCamera(&camera);
		int width, height;
		Core::Engine::Get().GetMainWindow()->GetSize(width, height);
		OnWindowResize(width, height);
		mRenderer = mProject.BuildRenderer(mGeometry);
		for (auto entity : reg.view<SkyboxComponent>())
			if (reg.get<SkyboxComponent>(entity).Enabled)
			{
				mSky = std::make_unique<EditorSkybox>();
				if (!mSky->Initialize(camera)) throw std::runtime_error("Unable to create skybox");
				break;
			}
		if (mDocument.NeedsPhysics()) ActivatePhysics();
		if (mDocument.NeedsAudio()) ActivateAudio();
		UI::UIModule::Get().GetContext().SetInputMode(UI::InputMode::GameOnly);
		if (mDocument.NeedsScripts()) ActivateScripts();
		Core::Engine::Get().GetMainWindow()->SetTitle(mDocument.ProjectName + " - " + (mDocument.Levels.size() > 1 ? mDocument.SceneName : "Play"));
		NUCLEAR_INFO("[Player] Game scene ready: {0}", mDocument.SceneName);
	}
	void ProjectPlayer::ActivatePhysics(entt::entity only)
	{
		auto& reg = GetScene().GetRegistry();
		if (!mPhysics) mPhysics = std::make_unique<Systems::PhysXSystem>(Systems::PhysXSystemDesc{{0, -9.81f, 0}});
        mPhysics->OnContact = [&reg](Uint32 first, Uint32 second, bool trigger, bool enter) {
            auto dispatch = [&](Uint32 id, Uint32 other) {
                auto entity = static_cast<entt::entity>(id);
                auto* script = reg.valid(entity) ? reg.try_get<Components::ScriptComponent>(entity) : nullptr;
                if (script)
                {
                    const char* method = trigger ? (enter ? "TriggerEnter" : "TriggerExit") :
                        (enter ? "CollisionEnter" : "CollisionExit");
                    script->GetScriptingObject().CallEntityMethod(method, other);
                }
            };
            dispatch(first, second);
            dispatch(second, first);
        };
        for (auto id : reg.view<Components::EntityInfoComponent>())
		{
			if (only != entt::null && id != only) continue;
			auto* body = reg.try_get<Components::RigidBodyComponent>(id);
			if (!body && !reg.all_of<Components::ColliderComponent>(id)) continue;
			const auto& transform = reg.get<Components::EntityInfoComponent>(id).mTransform;
			auto half = Math::max(Math::abs(transform.GetLocalScale()) * .5f, Math::Vector3(.005f));
			if (auto* mesh = reg.try_get<Components::MeshComponent>(id); mesh && mesh->GetMesh() == Assets::DefaultMeshes::GetPlaneAsset())
				half = {25.f * half.x, .025f, 25.f * half.z};
			PhysX::BoxGeometry box(half, transform);
			Components::ColliderDesc desc;
			desc.mGeo = &box;
			desc.mShape = Components::ColliderShape::Box;
			reg.get_or_emplace<Components::ColliderComponent>(id).Create(desc);
			ECS::Entity entity(reg, id);
			if (body)
			{
				const bool kinematic = body->isKinematic;
				reg.emplace_or_replace<Components::RigidBodyComponent>(id, transform);
				mPhysics->SetColliderForRigidBody(entity);
				if (kinematic) reg.get<Components::RigidBodyComponent>(id).SetisKinematic(true);
			}
			mPhysics->AddActor(entity);
			reg.get<Components::ColliderComponent>(id).SetValid(true);
		}
	}
	void ProjectPlayer::ActivateAudio(entt::entity only)
	{
		auto& reg = GetScene().GetRegistry();
		if (!mAudio) mAudio = std::make_unique<Systems::AudioSystem>();
		for (auto entity : reg.view<Components::EntityInfoComponent>())
		{
			if (only != entt::null && entity != only) continue;
			if (reg.remove<Components::AudioListenerComponent>(entity)) reg.emplace<Components::AudioListenerComponent>(entity);
			auto* source = reg.try_get<Components::AudioSourceComponent>(entity);
			if (!source || source->ClipPath.empty()) continue;
			auto path = source->ClipPath;
			Assets::AudioClipImportingDesc desc;
			desc.mCommonOptions.mAsyncImport = false;
			desc.mCommonOptions.mLoadOnly = true;
			auto* clip = GetAssetManager().Import<Assets::AudioClip>(path, desc);
			if (!clip || !clip->GetBufferID()) throw std::runtime_error("Could not load clip: " + path);
			reg.remove<Components::AudioSourceComponent>(entity);
			auto& active = reg.emplace<Components::AudioSourceComponent>(entity, clip);
			if (!active.GetSourceID()) throw std::runtime_error("Could not create audio source: " + path);
			active.Play();
		}
	}
	void ProjectPlayer::ActivateScripts(entt::entity only)
	{
		auto& reg = GetScene().GetRegistry();
		std::vector<std::pair<entt::entity, Assets::Script*>> bindings;
		std::map<std::string, Assets::Script*> classes;
		for (auto entity : reg.view<Components::ScriptComponent>())
		{
			if (only != entt::null && entity != only) continue;
			const auto& name = reg.get<Components::ScriptComponent>(entity).ClassName;
			if (name.empty()) continue;
			auto& asset = classes[name];
			if (!asset)
			{
				asset = &Assets::AssetLibrary::Get().mImportedScripts.AddAsset();
				if (!Scripting::ScriptingModule::Get().CreateScriptAsset(asset, name))
					throw std::runtime_error("Unknown C# entity class: " + name);
			}
			bindings.emplace_back(entity, asset);
		}
		if (!mScripts)
		{
			mScripts = std::make_unique<Systems::ScriptingSystem>();
			mScripts->Initialize();
		}
		for (const auto& [entity, asset] : bindings)
		{
			auto fields = reg.get<Components::ScriptComponent>(entity).Fields;
			reg.remove<Components::ScriptComponent>(entity);
			auto& script = reg.emplace<Components::ScriptComponent>(entity, asset, std::move(fields));
			if (!script.GetScriptingObject().IsValid()) throw std::runtime_error("Could not instantiate C# behavior");
		}
	}
    bool ProjectPlayer::SetGamePaused(Uint32 owner, bool paused)
    {
        auto entity = static_cast<entt::entity>(owner);
        if (!GetScene().GetRegistry().all_of<Components::ScriptComponent>(entity)) return false;
        if (mPauseOwner != entt::null && mPauseOwner != entity) return false;
        mPauseOwner = paused ? entity : entt::null;
        return true;
    }
    Uint32 ProjectPlayer::SpawnPrefab(const std::string& name, float x, float y, float z)
    {
        auto found = mTemplates.find(name);
        if (found == mTemplates.end() || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return UINT32_MAX;
        auto entity = GetScene().CreateEntity(found->second.GetName(), ECS::Transform({x, y, z})).GetEntityID();
        mSpawns.emplace_back(entity, found->second);
        return static_cast<Uint32>(entity);
    }
    bool ProjectPlayer::RequestLevel(const std::string& name)
    {
        if (name.empty() || !mNextLevel.empty() || !mDocument.Levels.contains(name)) return false;
        mNextLevel = name;
        return true;
    }

    void ProjectPlayer::Update(float seconds)
	{
		if (Platform::Input::Get().IsKeyPressed(Platform::Input::KEYCODE_ESCAPE))
			Core::Engine::Get().GetMainWindow()->RequestClose();
		auto& reg = GetScene().GetRegistry();
		seconds = std::clamp(seconds, 0.f, .1f);
		if (mPauseOwner != entt::null)
        {
            if (auto* script = reg.try_get<Components::ScriptComponent>(mPauseOwner); script && script->GetScriptAsset())
                script->GetScriptingObject().CallMethod(script->GetScriptAsset()->mOnUpdateMethod, 0.f);
            else mPauseOwner = entt::null;
            if (!mNextLevel.empty()) Core::Engine::Get().EndAfterFrame();
            return;
        }
        if (mScripts) mScripts->Update(seconds);
		if (!mNextLevel.empty()) Core::Engine::Get().EndAfterFrame();
        for (auto& [id, state] : std::exchange(mSpawns, {}))
        {
            if (!reg.valid(id)) continue;
            auto& info = reg.get<Components::EntityInfoComponent>(id);
            const auto position = info.mTransform.GetLocalPosition();
            const auto identity = info.PersistentId;
            state.Restore(id);
            info.PersistentId = identity;
            info.ParentId.clear();
            info.mTransform.SetPosition(position);
            if (reg.any_of<Components::RigidBodyComponent, Components::ColliderComponent>(id)) ActivatePhysics(id);
            if (reg.any_of<Components::AudioSourceComponent, Components::AudioListenerComponent>(id)) ActivateAudio(id);
            if (reg.all_of<Components::ScriptComponent>(id)) ActivateScripts(id);
        }
		if (mPhysics)
		{
			constexpr float step = 1.f / 60.f;
			mPhysicsTime += seconds;
			while (mPhysicsTime >= step)
			{
				mPhysics->Update(step);
				mPhysicsTime -= step;
			}
		}
		if (mAudio && seconds > 0) mAudio->Update(seconds);
		auto& camera = reg.get<Components::CameraComponent>(mCamera);
		GetScene().SetMainCamera(&camera);
		auto& transform = reg.get<Components::EntityInfoComponent>(mCamera).mTransform;
		GetScene().UpdateTransforms();
		camera.SetPosition(transform.GetWorldPosition());
		camera.SetOrientation(transform.GetWorldRotation());
		camera.UpdateBuffer();
	}
	void ProjectPlayer::Render(float seconds)
	{
		auto& camera = *GetScene().GetMainCamera();
		mRenderer->Update(seconds);
		if (mSky) mSky->Render(camera);
		Rendering::RenderingModule::Get().RenderToScreen(camera.GetColorRT().GetSRV());
	}
	void ProjectPlayer::OnWindowResize(int width, int height)
	{
		if (width <= 0 || height <= 0) return;
		Graphics::GraphicsModule::Get().ResizeSwapChain(width, height);
		Rendering::RenderingModule::Get().ResizeRTs(width, height);
		if (auto* camera = GetScene().GetMainCamera())
		{
			camera->ResizeRTs(width, height);
			camera->SetProjectionMatrix(Math::perspective(Math::radians(45.f), float(width) / height, .1f, 1000.f));
		}
	}
	void ProjectPlayer::Shutdown()
	{
		auto& scene = GetScene();
		auto& reg = scene.GetRegistry();
		mScripts.reset();
		if (mPhysics)
			for (auto entity : reg.view<Components::EntityInfoComponent>()) mPhysics->ReleaseActor(reg, entity);
		mRenderer.reset();
		mSky.reset();
		scene.SetMainCamera(static_cast<Components::CameraComponent*>(nullptr));
		scene.Clear();
		mPhysics.reset();
		mAudio.reset();
	}
}
