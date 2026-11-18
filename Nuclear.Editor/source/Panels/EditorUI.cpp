#include <GameExporter.h>
#include <iomanip>
#include <EditorUI.h>
#include <ProjectDocument.h>
#include <Nuclear.Platform/FileDialog.h>
#include <Nuclear.Platform/Platform.h>
#include <fstream>
#include <EditorHierarchyRow.h>
#include <EditorAssetPicker.h>
#include <EditorTitleBar.h>
#include <SceneSettings.h>
#include <EditorAddMenu.h>
#include <EditorSkybox.h>
#include <EditorSelectionRenderer.h>
#include <SkyboxComponent.h>
#include <EditorWidgets.h>
#include <EditorToolbar.h>
#include <AssetDocument.h>
#include <MeshPreviewViewport.h>
#include <MeshPlacementPreview.h>
#include <AboutDocument.h>
#include <VisualScriptDocument.h>
#include <EditorSettings.h>
#include <EditorSettingsDocument.h>
#include <GameViewPort.h>
#include <Fallbacks/FallbacksModule.h>
#include <Rendering/RenderingModule.h>
#include <Profiling/ProfilingModule.h>
#include <Profiling/MemoryProfiler.h>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace Nuclear::Editor
{
	namespace
	{
		constexpr std::array<GizmoMode, 4> ViewportModes = {
			GizmoMode::Select, GizmoMode::Move, GizmoMode::Rotate, GizmoMode::Scale};
	}

	EditorUI::EditorUI(Project& project) : mProject(project) {}
	EditorUI::~EditorUI() = default;

	UI::UIContext* EditorUI::GetHierarchyMenuContext() const
	{
		if (pHierarchyMenuContext == &UI::UIModule::Get().GetContext()) return pHierarchyMenuContext;
		if (pWorkspace)
			for (const auto& window : pWorkspace->GetWindows())
				if (pHierarchyMenuContext == &window->GetContext()) return pHierarchyMenuContext;
		return nullptr;
	}

	void EditorUI::CloseHierarchyMenu()
	{
		const auto id = std::exchange(mHierarchyMenu, 0);
		if (auto* context = GetHierarchyMenuContext(); context && id) context->RemoveFromViewport(id);
		pHierarchyMenuContext = nullptr;
		mHierarchyMenuEntity = entt::null;
	}

	void EditorUI::OpenHierarchyMenu(entt::entity entity, UI::UIContext& context, UI::Vec2 point)
	{
		auto& registry = Core::Scene::Get().GetRegistry();
		if (mModal || entity == mCamera || !registry.valid(entity) || !registry.all_of<Components::EntityInfoComponent>(entity)) return;
		CloseHierarchyMenu();
		CloseAddMenu();
		pTitleBar->CloseMenu();
		pViewport->CancelInteraction();
		mSelected = entity;
		mHierarchyDirty = mInspectorDirty = true;
		pInspector->SetOffset(0);
		EditorMenuPopup::Items items{
			{"Duplicate", [this, entity]() { DuplicateEntity(entity); }},
			{"Copy", [this, entity]() { CopyEntity(entity); }},
			{"Delete", [this, entity]() { DeleteEntity(entity); }}
		};
		auto popup = std::make_unique<EditorMenuPopup>(UI::Rect{point.x, point.y, 0, 0}, items,
			[this]() { CloseHierarchyMenu(); },
			[this](UI::Vec2, bool clicked) { if (clicked) CloseHierarchyMenu(); }, [](int) {});
		mHierarchyMenu = popup->GetId();
		pHierarchyMenuContext = &context;
		mHierarchyMenuEntity = entity;
		context.AddToViewport(std::move(popup), 60);
		context.SetFocus(mHierarchyMenu);
	}

	void EditorUI::DeleteEntity(entt::entity entity)
	{
		auto& registry = Core::Scene::Get().GetRegistry();
		if (mModal || entity == mCamera || !registry.valid(entity)) return;
		CloseHierarchyMenu();
		pViewport->SetSelectedEntity(entt::null);
		mHistory.Edit(entity, "Delete entity", [&]() { registry.destroy(entity); });
		Core::Scene::Get().SetMainCamera(registry.try_get<Components::CameraComponent>(mCamera));
		mSelected = entt::null;
		mHierarchyDirty = mInspectorDirty = mRendererDirty = true;
	}

	void EditorUI::CopyEntity(entt::entity entity)
	{
		if (mModal || entity == mCamera) return;
		CloseHierarchyMenu();
		pViewport->CancelInteraction();
		const auto state = EditorEntityState::Capture(entity);
		if (!state.Exists) return;
		mCopiedEntity = state;
		mNotice = "Entity copied / Ctrl+V or Edit / Paste";
	}

	void EditorUI::DuplicateEntity(entt::entity entity)
	{
		if (mModal || entity == mCamera) return;
		pViewport->CancelInteraction();
		PasteEntity(EditorEntityState::Capture(entity));
	}

	void EditorUI::PasteEntity(const EditorEntityState& state)
	{
		if (mModal || !state.Exists) return;
		CloseHierarchyMenu();
		pViewport->CancelInteraction();
		auto& registry = Core::Scene::Get().GetRegistry();
		const auto base = (state.GetName().empty() ? std::string("Entity") : state.GetName()) + " Copy";
		auto exists = [&](const std::string& name) {
			for (auto entity : registry.view<Components::EntityInfoComponent>())
				if (registry.get<Components::EntityInfoComponent>(entity).mName == name) return true;
			return false;
		};
		std::string name = base;
		for (size_t suffix = 2; exists(name); ++suffix) name = base + " " + std::to_string(suffix);
		const auto copy = state.CreateCopy(name);
		Core::Scene::Get().SetMainCamera(registry.try_get<Components::CameraComponent>(mCamera));
		if (copy == entt::null) return;
		mSelected = copy;
		mHistory.RecordCreated(copy);
		mHierarchyDirty = mInspectorDirty = mRendererDirty = true;
		mRevealSelection = true;
		pInspector->SetOffset(0);
		pWorkspace->Show("viewport");
		mNotice = "Entity copied into scene / Undo removes this copy";
	}

	void EditorUI::CancelMeshPlacement()
	{
		if (pMeshPlacement) pMeshPlacement->Cancel();
		mMeshPlacementPoint.reset();
		pMeshPlacementContext = nullptr;
		mMeshPlacementDropped = false;
	}

	void EditorUI::DragMesh(const Core::UUID& mesh, UI::UIContext& source, UI::Vec2 point, bool dropped)
	{
		if (!pMeshPlacement || !pViewport || mModal)
		{
			CancelMeshPlacement();
			return;
		}
		pMeshPlacement->Begin(mesh);
		mMeshPlacementPoint.reset();
		auto* target = pViewport->GetContext();
		if (!target || !pViewport->IsVisible() || !pViewport->IsEnabled() ||
			target->GetInputMode() == UI::InputMode::GameOnly)
		{
			if (dropped) CancelMeshPlacement();
			return;
		}
		auto windowFor = [this](UI::UIContext* context) -> Platform::Window* {
			if (context == &UI::UIModule::Get().GetContext()) return Core::Engine::Get().GetMainWindow();
			for (const auto& window : pWorkspace->GetWindows())
				if (context == &window->GetContext()) return &window->GetWindow();
			return nullptr;
		};
		if (&source != target)
		{
			auto* originWindow = windowFor(&source);
			auto* targetWindow = windowFor(target);
			int x = int(std::lround(point.x * source.GetScale()));
			int y = int(std::lround(point.y * source.GetScale()));
			if (!originWindow || !targetWindow || !originWindow->ClientToScreen(x, y) || !targetWindow->ScreenToClient(x, y))
			{
				if (dropped) CancelMeshPlacement();
				return;
			}
			point = {x / target->GetScale(), y / target->GetScale()};
		}
		target->Layout();
		if (!pViewport->GetBounds().Contains(point))
		{
			if (dropped) CancelMeshPlacement();
			return;
		}
		mMeshPlacementPoint = point;
		mMeshPlacementBounds = pViewport->GetBounds();
		pMeshPlacementContext = target;
		mMeshPlacementDropped = dropped;
	}

	void EditorUI::UpdateMeshPlacement()
	{
		if (!pMeshPlacement || !pMeshPlacement->GetMesh()) return;
		const auto bounds = pViewport->GetBounds();
		if (mModal || !pViewport->IsVisible() || !pViewport->IsEnabled() ||
			(mMeshPlacementPoint && (pViewport->GetContext() != pMeshPlacementContext ||
			bounds.x != mMeshPlacementBounds.x || bounds.y != mMeshPlacementBounds.y ||
			bounds.Width != mMeshPlacementBounds.Width || bounds.Height != mMeshPlacementBounds.Height)))
		{
			CancelMeshPlacement();
			return;
		}
		if (!mMeshPlacementPoint) return;
		auto* camera = Core::Scene::Get().GetMainCamera();
		const bool valid = camera && pMeshPlacement->Update(*camera, bounds, *mMeshPlacementPoint);
		if (!mMeshPlacementDropped) return;
		if (valid)
		{
			auto* mesh = Assets::AssetLibrary::Get().mImportedMeshes.GetAsset(*pMeshPlacement->GetMesh());
			if (mesh && mesh->GetState() == Assets::IAsset::State::Created)
			{
				auto entity = Core::Scene::Get().CreateEntity(mesh->GetName());
				entity.GetComponent<Components::EntityInfoComponent>().mTransform.SetPosition(pMeshPlacement->GetPosition());
				entity.AddComponent<Components::MeshComponent>(mesh, mProject.GetMeshMaterial(*mesh));
				mSelected = entity.GetEntityID();
				mHistory.RecordCreated(mSelected);
				mHierarchyDirty = mInspectorDirty = true;
				mRevealSelection = true;
				pInspector->SetOffset(0);
				mNotice = "Mesh placed / Undo removes this placement";
			}
		}
		CancelMeshPlacement();
	}

	void EditorUI::Initialize()
	{
		mHistory.OnSceneRestored = [this]() {
			mSceneSettingsDirty = mAssetsDirty = true;
		};
		mHistory.OnRestored = [this](entt::entity entity) {
			auto& scene = Core::Scene::Get();
			scene.SetMainCamera(scene.GetRegistry().try_get<Components::CameraComponent>(mCamera));
			mSelected = entity;
			mHierarchyDirty = mInspectorDirty = mRendererDirty = true;
			pViewport->SetSelectedEntity(entity);
			pInspector->SetOffset(0);
		};
		mProject.InitializeMaterials();
		pMaterial = mProject.GetDefaultMaterial();
		mProject.Initalize({"Untitled Project"});
		mProject.SetPath(std::filesystem::absolute(Assets::AssetLibrary::Get().GetPath()));
		mAssets.SetProject(&mProject);
		mAssets.OnChanged = [this]() {
			mAssetsDirty = true;
		};
		mAssets.OnEntityAdded = [this](entt::entity entity) {
			mHistory.RecordCreated(entity);
			mSelected = entity;
			mHierarchyDirty = mInspectorDirty = true;
			pInspector->SetOffset(0);
			pWorkspace->Show("viewport");
		};
		mAssets.OnDocumentOpened = [this](const std::filesystem::path& file) {
			if (ProjectDocument::IsMap(file)) OpenMap(file);
			else OpenProject(file);
		};
		mAssets.OnAssetOpened = [this](Assets::AssetType type, Core::UUID id) {
			mPendingDocuments.emplace_back(type, id);
		};
		pMeshPlacement = std::make_unique<MeshPlacementPreview>();
		mAssets.OnMeshDragged = [this](Core::UUID mesh, UI::UIContext& source, UI::Vec2 point, bool dropped) {
			DragMesh(mesh, source, point, dropped);
		};
		mAssets.OnMeshDragCancelled = [this]() { CancelMeshPlacement(); };
		mAssets.OnAssetChanged = [this](Assets::AssetType type, Core::UUID id, bool removed) {
			if (!pWorkspace) return;
			const auto key = "asset_" + std::to_string(static_cast<int>(type)) + "_" + id.str();
			if (removed)
				pWorkspace->Close(key);
			else if (auto* asset = AssetDocument::Resolve(type, id))
			{
				const auto title = AssetDocument::GetTitle(type, asset->GetName());
				pWorkspace->SetTitle(key, title);
				auto found = mAssetHeadings.find(key);
				if (found != mAssetHeadings.end() && found->second)
					found->second->SetText(title);
			}
		};
		NewScene(true);
		pSelectionRenderer = std::make_unique<EditorSelectionRenderer>();
		if (!pSelectionRenderer->Initialize(*Core::Scene::Get().GetMainCamera()))
			throw std::runtime_error("Editor selection renderer could not be initialized");
		int width = 0, height = 0;
		Core::Engine::Get().GetMainWindow()->GetSize(width, height);
		Resize(width, height);
		BuildWorkspace();
		CreateEntity(1);
		mHistory.Clear();
		auto& context = UI::UIModule::Get().GetContext();
		context.SetInputMode(UI::InputMode::UIOnly);
		context.OnPreviewEvent = [this, &context](const Platform::Event& event) {
			return ProcessShortcut(event, context);
		};
		Platform::Input::Get().SetMouseInputMode(Platform::Input::MouseInputMode::Normal);
	}

	void EditorUI::SaveProject(const std::filesystem::path& file)
	{
		if (!ProjectDocument::IsProject(file)) throw std::runtime_error("Use .nuclearproject for the main project");
		const auto target = std::filesystem::absolute(file).lexically_normal();
		const bool startup = mStartupMap.empty() || mMapFile == mStartupMap;
		auto map = mMapFile;
		if (!ProjectDocument::IsMap(map) || (startup && target != mProjectFile && !mProjectFile.empty()))
		{
			map = target.parent_path() / target.stem();
			map += ".NMap";
			if (std::filesystem::exists(map))
				map = target.parent_path() / (target.stem().string() + "-" + Core::UUID::CreateNewUUID().str() + ".NMap");
		}
		auto levels = mLevels;
		if (!startup)
		{
			const auto name = Core::Scene::Get().GetSceneAsset()->GetName();
			const bool registered = std::any_of(levels.begin(), levels.end(), [&](const auto& entry) {
				return entry.second == map;
			});
			if (!registered && (name == ProjectDocument::Read(mStartupMap).SceneName || !levels.emplace(name, map).second))
				throw std::runtime_error("Give the map a unique scene name before saving the project");
		}
		SaveMap(map);
		ProjectDocument manifest;
		manifest.ProjectName = mProject.GetProjectInfo().mProjectName;
		manifest.Assembly = mScriptAssembly;
		manifest.StartupMap = startup ? map : mStartupMap;
		manifest.Levels = std::move(levels);
		manifest.Prefabs = mPrefabs;
		manifest.ConvertLegacyLevels(target.parent_path());
		manifest.SaveManifest(target);
		mStartupMap = manifest.StartupMap;
		mLevels = std::move(manifest.Levels);
		mProjectFile = target;
		mProject.SetPath(target.parent_path());
		mNotice = "Saved " + target.string();
	}

	void EditorUI::SaveMap(const std::filesystem::path& file)
	{
		if (!ProjectDocument::IsMap(file)) throw std::runtime_error("Save maps with the .NMap extension");
		auto target = std::filesystem::absolute(file).lexically_normal();
		auto doc = ProjectDocument::Capture(mProject, mCamera, mGameCamera);
		doc.Prefabs = mMapPrefabs;
		const bool startup = mStartupMap.empty() || (mMapFile == mStartupMap &&
			(target == mMapFile || !ProjectDocument::IsMap(mMapFile)));
		if (!startup && target == mStartupMap) throw std::runtime_error("This filename belongs to the startup map");
		doc.Save(target, mProject);
		if (startup) mStartupMap = target;
		mMapFile = target;
		mAssetsDirty = true;
		mNotice = "Saved map " + target.string();
	}

	void EditorUI::OpenMap(const std::filesystem::path& file)
	{
		if (!ProjectDocument::IsMap(file)) throw std::runtime_error("Open a .NMap asset");
		auto doc = ProjectDocument::Read(file);
		ApplyMap(doc);
		mMapFile = std::filesystem::absolute(file).lexically_normal();
		mMapPrefabs = std::move(doc.Prefabs);
		if (mStartupMap.empty()) mStartupMap = mMapFile;
		mNotice = "Opened map " + mMapFile.string();
	}

	void EditorUI::OpenProject(const std::filesystem::path& file)
	{
		if (!ProjectDocument::IsProject(file)) throw std::runtime_error("Open a main .nuclearproject file");
		auto doc = ProjectDocument::Read(file);
		ApplyMap(doc);
		mProject.Initalize({doc.ProjectName});
		mProjectFile = std::filesystem::absolute(file).lexically_normal();
		mProject.SetPath(mProjectFile.parent_path());
		mMapFile = mStartupMap = doc.StartupMap.empty() ? mProjectFile : doc.StartupMap;
		mScriptAssembly = doc.Assembly;
		mLevels = std::move(doc.Levels);
		mPrefabs = std::move(doc.Prefabs);
		mMapPrefabs = std::move(doc.StartupPrefabs);
		mNotice = "Opened " + mProjectFile.string();
	}

	void EditorUI::ApplyMap(ProjectDocument& doc)
	{
		doc.Resolve(mProject);
		NewScene();
		auto& scene = Core::Scene::Get();
		auto& reg = scene.GetRegistry();
		// NewScene supplies one default light for empty authoring sessions.
		for (auto id : reg.view<Components::LightComponent>()) reg.destroy(id);
		mGameCamera = doc.Instantiate();
		scene.SetMainCamera(reg.try_get<Components::CameraComponent>(mCamera));
		auto settings = EditorSceneState::Capture();
		settings.Name = doc.SceneName;
		settings.ClearColor = doc.ClearColor;
		settings.Restore();
		if (!reg.view<SkyboxComponent>().empty() && !PrepareSkybox())
			throw std::runtime_error("Unable to initialize the map skybox");
		mRendererDirty = true;
	}

	void EditorUI::Play()
	{
		if (mPlayer) { mNotice = "Game already running"; return; }
		auto& reg = Core::Scene::Get().GetRegistry();
		if (!reg.valid(mGameCamera) || !reg.all_of<Components::CameraComponent>(mGameCamera))
			throw std::runtime_error("Select a camera entity, then Project > Use selected camera for Play");
		auto doc = ProjectDocument::Capture(mProject, mCamera, mGameCamera);
		doc.Assembly = mScriptAssembly;
		doc.Levels = mLevels;
		doc.Prefabs = mPrefabs;
		const auto directory = std::filesystem::temp_directory_path() / (".nuclear-play-" + Core::UUID::CreateNewUUID().str());
		if (!std::filesystem::create_directory(directory)) throw std::runtime_error("Could not create play directory");
		mPlayDirectory = directory;
		try
		{
			auto snapshot = std::filesystem::absolute(mPlayDirectory / "play.nuclearproject");
			std::erase_if(doc.Levels, [&](const auto& entry) { return entry.second == mMapFile || entry.first == doc.SceneName; });
			if (!mStartupMap.empty() && mMapFile != mStartupMap)
			{
				auto startup = ProjectDocument::Read(mStartupMap);
				if (startup.SceneName != doc.SceneName) doc.Levels.emplace(startup.SceneName, mStartupMap);
			}
			doc.StartupMap = mPlayDirectory / "play.NMap";
			doc.ConvertLegacyLevels(mPlayDirectory);
			auto map = doc;
			map.Prefabs = mMapPrefabs;
			map.Save(doc.StartupMap, mProject);
			doc.SaveManifest(snapshot);
			auto executable = Platform::GetExecutablePath().parent_path() / "Nuclear.Editor.exe";
			auto assets = std::filesystem::absolute(Assets::AssetLibrary::Get().GetPath());
			auto child = std::make_unique<Platform::ChildProcess>();
			child->Start(executable, {"--play", snapshot, assets}, mPlayDirectory);
			mPlayer = std::move(child);
			mNotice = "Playing / Escape or Stop ends the game";
		}
		catch (...) { FinishPlay(); throw; }
	}

	void EditorUI::Stop()
	{
		mStopping = true;
		if (mPlayer) mPlayer->RequestClose();
	}

	void EditorUI::FinishPlay()
	{
		mPlayer.reset();
		mStopping = false;
		if (mPlayDirectory.empty()) return;
		std::error_code error;
		std::filesystem::remove_all(mPlayDirectory, error); // Private directory created for this Play session.
		mPlayDirectory.clear();
	}

	void EditorUI::UpdateProjectActions()
	{
		try
		{
			if (mPlayer)
			{
				if (auto code = mPlayer->ExitCode())
				{
					std::string reason;
					std::ifstream diagnostic(mPlayDirectory / "play-error.txt");
					std::getline(diagnostic, reason);
					diagnostic.close();
					mNotice = *code == 0 ? "Play stopped" : "Play failed: " + (reason.empty() ? std::to_string(*code) : reason);
					if (*code != 0)
					{
						// Keep diagnostics when startup fails, including SDK/device and managed loader messages.
						mNotice += " / log: " + (mPlayDirectory / "NuclearEngine.log").string();
						std::error_code error;
						std::filesystem::remove(mPlayDirectory / "play.nuclearproject", error);
						std::filesystem::remove(mPlayDirectory / "play.NMap", error);
						mPlayDirectory.clear();
					}
					FinishPlay();
				}
				else if (mStopping) mPlayer->RequestClose();
			}
			const auto action = std::exchange(mProjectAction, ProjectAction::None);
			if (action == ProjectAction::None) return;
			if (action == ProjectAction::Play) { Play(); return; }
			if (action == ProjectAction::Stop) { Stop(); return; }
			auto& window = *Core::Engine::Get().GetMainWindow();
			const auto base = mProjectFile.empty() ? mProject.GetPath() : mProjectFile.parent_path();
			std::string error;
			if (action == ProjectAction::Assembly)
			{
				auto file = Platform::FileDialog::OpenFile(window, "Select gameplay assembly", "*.dll", base, error);
				if (!file.empty()) { mScriptAssembly = file; mNotice = "Gameplay assembly: " + file.string(); }
			}
			else if (action == ProjectAction::Export)
            {
                if (mProjectFile.empty()) throw std::runtime_error("Save the project before building the game");
                auto output = Platform::FileDialog::SaveFile(window, "Name the new game folder", "*.*", base, "", error);
                if (!output.empty())
                {
                    SaveProject(mProjectFile);
                    auto release = Platform::GetExecutablePath().parent_path().parent_path() / "ReleaseX64";
                    GameExporter::Build(mProject, ProjectDocument::Read(mProjectFile), release,
                        Assets::AssetLibrary::Get().GetPath(), output);
                    mNotice = "Built " + (output / "Game.exe").string();
                }
            }
            else if (action == ProjectAction::SavePrefab || action == ProjectAction::PlacePrefab)
            {
                if (action == ProjectAction::SavePrefab && (mSelected == mCamera || mSelected == entt::null))
                    throw std::runtime_error("Select a scene entity to save as a prefab");
                auto file = action == ProjectAction::SavePrefab
                    ? Platform::FileDialog::SaveFile(window, "Save selected prefab", "*.nuclearprefab", base, "nuclearprefab", error)
                    : Platform::FileDialog::OpenFile(window, "Place prefab", "*.nuclearprefab", base, error);
                if (!file.empty())
                {
                    if (action == ProjectAction::SavePrefab) ProjectDocument::CapturePrefab(mSelected).Save(file, mProject);
                    else
                    {
                        auto doc = ProjectDocument::Read(file);
                        doc.Resolve(mProject);
                        PasteEntity(doc.Template());
                    }
                    mPrefabs[file.stem().string()] = file;
                    mNotice = "Registered prefab " + file.stem().string();
                }
            }
            else if (action == ProjectAction::SaveSurface || action == ProjectAction::LoadSurface)
            {
                auto& reg = Core::Scene::Get().GetRegistry();
                auto* mesh = reg.try_get<Components::MeshComponent>(mSelected);
                if (!mesh) throw std::runtime_error("Select an entity with a mesh");
                bool save = action == ProjectAction::SaveSurface;
                auto path = save ? Platform::FileDialog::SaveFile(window, "Save material preset", "*.nuclearmaterial", base, "nuclearmaterial", error)
                    : Platform::FileDialog::OpenFile(window, "Apply material preset", "*.nuclearmaterial", base, error);
                if (!path.empty() && save)
                {
                    auto temp = path.parent_path() / (Core::UUID::CreateNewUUID().str() + ".tmp");
                    try
                    {
                        std::ofstream out(temp);
                        out.exceptions(std::ios::badbit | std::ios::failbit);
                        out.imbue(std::locale::classic());
                        out << "NuclearMaterial 1\\n" << std::setprecision(9) << mesh->Tint.r << ' ' << mesh->Tint.g << ' ' << mesh->Tint.b
                            << ' ' << mesh->Metallic << ' ' << mesh->Roughness;
                        out.close();
                        std::filesystem::rename(temp, path);
                    }
                    catch (...) { std::error_code ignored; std::filesystem::remove(temp, ignored); throw; }
                }
                else if (!path.empty())
                {
                    std::ifstream input(path);
                    input.imbue(std::locale::classic());
                    std::string magic;
                    int version;
                    Math::Vector3 tint;
                    float metal, rough;
                    if (!(input >> magic >> version >> tint.r >> tint.g >> tint.b >> metal >> rough) ||
                        magic != "NuclearMaterial" || version != 1 || !(input >> std::ws).eof() ||
                        !(tint.r >= 0 && tint.r <= 1 && tint.g >= 0 && tint.g <= 1 && tint.b >= 0 && tint.b <= 1 &&
                            metal >= -1 && metal <= 1 && rough >= -1 && rough <= 1))
                        throw std::runtime_error("Invalid material preset");
                    mHistory.Edit(mSelected, "Apply material preset", [&]() { mesh->Tint = tint; mesh->Metallic = metal; mesh->Roughness = rough; });
                    RefreshInspector();
                }
            }
            else if (action == ProjectAction::OpenMap)
			{
				auto file = Platform::FileDialog::OpenFile(window, "Open map", "*.NMap", base, error);
				if (!file.empty()) OpenMap(file);
			}
			else if (action == ProjectAction::SaveMap || action == ProjectAction::SaveMapAs)
			{
				auto file = mMapFile;
				if (!ProjectDocument::IsMap(file) || action == ProjectAction::SaveMapAs)
					file = Platform::FileDialog::SaveFile(window, "Save map", "*.NMap", base, "NMap", error);
				if (!file.empty()) SaveMap(file);
			}
			else if (action == ProjectAction::AddLevel)
            {
                auto file = Platform::FileDialog::OpenFile(window, "Link a saved map", "*.NMap", base, error);
                if (!file.empty())
                {
                    if (!ProjectDocument::IsMap(file) || file == mStartupMap) throw std::runtime_error("Choose a separate .NMap asset");
                    const auto level = ProjectDocument::Read(file);
                    const auto found = mLevels.find(level.SceneName);
                    if (level.SceneName.empty() || (found != mLevels.end() && found->second != file))
                        throw std::runtime_error("Choose a level with a unique scene name");
                    mLevels[level.SceneName] = file;
                    mAssetsDirty = true;
                    mNotice = "Linked level " + level.SceneName;
                }
            }
            else if (action == ProjectAction::Open)
			{
				auto file = Platform::FileDialog::OpenFile(window, "Open project", "*.nuclearproject", base, error);
				if (!file.empty()) OpenProject(file);
			}
			else
			{
				auto file = mProjectFile;
				if (file.empty() || action == ProjectAction::SaveAs)
					file = Platform::FileDialog::SaveFile(window, "Save project", "*.nuclearproject", base, "nuclearproject", error);
				if (!file.empty()) SaveProject(file);
			}
			if (!error.empty()) mNotice = error;
		}
		catch (const std::exception& e) { mNotice = e.what(); }
	}

	void EditorUI::NewScene(bool defaultSkybox)
	{
		mGameCamera = entt::null;
		if (pWorkspace) pWorkspace->GetColorPicker().Close();
		CloseHierarchyMenu();
		mCopiedEntity.reset();
		if (pAssets && pAssets->GetContext()) pAssets->GetContext()->CancelSubtree(*pAssets);
		CancelMeshPlacement();
		mPendingPick.reset();
		mRevealSelection = false;
		CloseAddMenu();
		mPendingSkybox = false;
		if (pViewport)
			pViewport->SetSelectedEntity(entt::null);
		mHistory.Clear();
		pRenderer.reset();
		mInspector.Reset();
		mSceneSettingsDirty = true;
		auto& scene = Core::Scene::Get();
		scene.SetMainCamera(static_cast<Components::CameraComponent*>(nullptr));
		scene.Clear();
		mProject.AddNewScene();
		auto camera = scene.CreateEntity("Editor Camera");
		mCamera = camera.GetEntityID();
		Components::CameraComponentDesc desc;
		desc.mProjection = Math::perspective(Math::radians(45.0f), 1.6f, .1f, 1000.f);
		desc.mYaw = -116.565f;
		desc.mPitch = -16.602f;
		desc.mRTDesc = Rendering::RenderingModule::Get().GetFinalRT().GetDesc();
		desc.mRTDesc.ColorTexFormat = Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB;
		desc.mDepthRTDesc = Rendering::RenderingModule::Get().GetFinalDepthRT().GetDesc();
		auto& component = camera.AddComponent<Components::CameraComponent>(desc);
		component.SetPosition({3, 2, 6});
		component.UpdateBuffer();
		component.mRTClearColor = Graphics::Color(.025f, .04f, .055f, 1.0f);
		scene.SetMainCamera(&component);
		auto keyLight = scene.CreateEntity("Key Light");
		auto& light =
			keyLight.AddComponent<Components::LightComponent>(Components::LightComponent::Type::Directional);
		light.SetDirection({-.4f, -1.0f, -.3f});
		light.SetIntensity(1.0f);
		if (defaultSkybox)
		{
			if (!PrepareSkybox()) throw std::runtime_error("Unable to initialize the default skybox");
			scene.CreateEntity("Skybox").AddComponent<SkyboxComponent>();
		}
		mSelected = entt::null;
		RebuildRenderer();
		mHierarchyDirty = mInspectorDirty = mAssetsDirty = true;
		mNotice = "New scene / use File > Save project";
		if (pWorkspace)
			pWorkspace->Show("viewport");
	}

	void EditorUI::RebuildRenderer()
	{
		pRenderer = mProject.BuildRenderer(mGeometry);
		mRendererDirty = false;
	}

	void EditorUI::CreateEntity(int shape)
	{
		auto& scene = Core::Scene::Get();
		ECS::Entity entity;
		if (shape == 1)
			entity = scene.CreateBox(pMaterial, {}, false);
		else if (shape == 2)
			entity = scene.CreateSphere(pMaterial, {}, false);
		else if (shape == 3)
			entity = scene.CreatePlane(pMaterial, {}, false);
		else
			entity = scene.CreateEntity("Entity");
		mSelected = entity.GetEntityID();
		mHistory.RecordCreated(mSelected);
		if (pInspector)
			pInspector->SetOffset(0);
		mHierarchyDirty = mInspectorDirty = true;
		mNotice = "Entity created / select it to edit properties";
		if (pWorkspace)
			pWorkspace->Show("viewport");
	}

	void EditorUI::BuildWorkspace()
	{
		auto root = std::make_unique<UI::Border>();
		root->SetBrush({nullptr, {.035f, .045f, .06f, 1}});
		auto& frame = root->Add<UI::VerticalBox>();
		pTitleBar = &frame.Add<EditorTitleBar>(*Core::Engine::Get().GetMainWindow());
		auto& body = frame.Add<UI::Border>();
		body.SetBrush({nullptr, {.035f, .045f, .06f, 1}});
		body.SetPadding({8, 8, 8, 8});
		Fill(frame, body);
		auto& shell = body.Add<UI::VerticalBox>();
		shell.SetSpacing(8);
		auto& documents = shell.Add<EditorTabStrip>();
		auto& toolbar = shell.Add<UI::Border>();
		pSceneToolbar = &toolbar;
		toolbar.SetBrush({nullptr, {.06f, .075f, .095f, 1}});
		toolbar.SetPadding({8, 6, 8, 6});
		auto& actions = toolbar.Add<UI::HorizontalBox>();
		actions.SetSpacing(8);
		auto& newProject = Action(actions, "New project", [this]() {
			pTitleBar->CloseMenu();
			auto& context = UI::UIModule::Get().GetContext();
			auto modal = std::make_unique<UI::Border>();
			modal->SetBrush({nullptr, {.045f, .06f, .08f, 1}});
			modal->SetPadding({300, 150, 300, 150});
			auto& form = modal->Add<UI::VerticalBox>();
			form.SetSpacing(16);
			Label(form, "CREATE PROJECT", 26);
			Label(form, "Creates a project with a new startup scene.").SetWrap(true);
			auto& name = form.Add<UI::TextBox>("New Project");
			const auto id = modal->GetId();
			mModal = id;
			auto& row = form.Add<UI::HorizontalBox>();
			row.SetSpacing(10);
			Action(row, "Create project", [this, &name, id]() {
				mProjectFile.clear();
				mMapFile.clear();
				mStartupMap.clear();
				mMapPrefabs.clear();
				mLevels.clear();
				mPrefabs.clear();
				mScriptAssembly.clear();
				mProject.Initalize({name.GetText().empty() ? "New Project" : name.GetText()});
				NewScene(true);
				UI::UIModule::Get().GetContext().RemoveFromViewport(id);
			});
			Action(row, "Cancel", [id]() {
				UI::UIModule::Get().GetContext().RemoveFromViewport(id);
			});
			// Disable the shell so Tab navigation and pointer input stay inside the modal.
			if (auto* shell = context.Find(mRoot))
				shell->SetEnabled(false);
			pWorkspace->SetInteractionEnabled(false);
			form.Add<UI::TextBlock>("Current scene changes will be discarded.");
			context.AddToViewport(std::move(modal), 100);
		});
		auto& newScene = Action(actions, "New scene", [this]() {
			mMapFile.clear();
			mMapPrefabs.clear();
			NewScene();
		});
		Action(actions, "Play", [this]() { mProjectAction = ProjectAction::Play; });
		Action(actions, "Stop", [this]() { mProjectAction = ProjectAction::Stop; });
		auto& add = Action(actions, "Add", {});
		add.OnClicked = [this, &add]() { OpenAddMenu(add.GetBounds()); };
		auto& documentHost = shell.Add<EditorDockArea>();
		Fill(shell, documentHost);
		pWorkspace = std::make_unique<EditorWorkspace>(UI::UIModule::Get().GetContext(), documents, documentHost);

		auto hierarchyWidget = std::make_unique<UI::VerticalBox>();
		auto& hierarchy = *hierarchyWidget;
		hierarchy.SetPadding({12, 10, 12, 10});
		hierarchy.SetSpacing(8);
		auto& filter = hierarchy.Add<UI::TextBox>();
		pHierarchyFilter = &filter;
		filter.OnChanged = [this](const std::string& text) {
			mFilter = text;
			mHierarchyDirty = true;
		};
		Label(hierarchy, "Filter entities by name", 12);
		pHierarchy = &hierarchy.Add<UI::ScrollBox>();
		Fill(hierarchy, *pHierarchy);
		pWorkspace->Add("hierarchy", "Hierarchy", std::move(hierarchyWidget), EditorDockRegion::Left);

		auto sceneWidget = std::make_unique<UI::VerticalBox>();
		auto& scene = *sceneWidget;
		scene.SetPadding({12, 10, 12, 10});
		scene.SetSpacing(10);
		pSceneTitle = &Label(scene, "Untitled Scene", 20);
		auto& viewActions = scene.Add<EditorToolbar>();
		const char* modes[] = {"Select", "Move", "Rotate", "Scale"};
		for (size_t mode = 0; mode < ViewportModes.size(); ++mode)
			mGizmoButtons[mode] = &Action(viewActions, modes[mode], [this, mode]() {
				pViewport->SetGizmoMode(ViewportModes[mode]);
			});
		auto& frameSelected = Action(viewActions, "Frame selected", [this]() {
			auto& scene = Core::Scene::Get();
			auto& registry = scene.GetRegistry();
			auto* info = registry.valid(mSelected) ? registry.try_get<Components::EntityInfoComponent>(mSelected) : nullptr;
			if (info)
			{
				auto* camera = scene.GetMainCamera();
				camera->SetPosition(info->mTransform.GetLocalPosition() - camera->GetFrontView() * 6.0f);
			}
		});
		auto& deleteSelected = Action(viewActions, "Delete selected", [this]() {
			DeleteEntity(mSelected);
		});
		pViewport = &scene.Add<GameViewPort>();
		pViewport->OnSelectionRequested = [this](UI::Vec2 position) {
			mPendingPick = position;
			mPickBounds = pViewport->GetBounds();
		};
		pViewport->OnTransformChanged = [this]() {
			mInspectorDirty = true;
		};
		pViewport->OnTransformCommitted = [this](entt::entity entity, const ECS::Transform& before) {
			mHistory.RecordTransform(entity, before);
		};
		Fill(scene, *pViewport);
		Label(scene, "WASD: fly / Q: select / G/E/R: transform / Right: look / Wheel: dolly / Esc: cancel",
			  12).SetWrap(true);
		pWorkspace->SetViewportContent(std::move(sceneWidget));

		auto inspectorWidget = std::make_unique<UI::VerticalBox>();
		auto& inspector = *inspectorWidget;
		inspector.SetPadding({12, 10, 12, 10});
		pInspector = &inspector.Add<UI::ScrollBox>();
		Fill(inspector, *pInspector);
		pWorkspace->Add("inspector", "Inspector", std::move(inspectorWidget), EditorDockRegion::Right);
		auto settings = std::make_unique<SceneSettings>(mHistory, [this]() {
			mSceneSettingsDirty = mAssetsDirty = true;
		}, pWorkspace->GetColorPicker());
		pSceneSettings = settings.get();
		pWorkspace->Add("scene-settings", "Scene settings", std::move(settings), EditorDockRegion::Right);
		pWorkspace->Close("scene-settings");

		auto assetsWidget = std::make_unique<UI::VerticalBox>();
		auto& assets = *assetsWidget;
		assets.SetPadding({5, 3, 5, 2});
		pAssets = &assets.Add<UI::ContentWidget>();
		Fill(assets, *pAssets);
		pWorkspace->Add("assets", "Asset Browser", std::move(assetsWidget), EditorDockRegion::BottomLeft);

		auto diagnosticsWidget = std::make_unique<UI::VerticalBox>();
		auto& diagnostics = *diagnosticsWidget;
		diagnostics.SetPadding({12, 10, 12, 10});
		diagnostics.SetSpacing(8);
		auto& controls = diagnostics.Add<UI::HorizontalBox>();
		controls.SetSpacing(8);
		auto& capture = controls.Add<UI::CheckBox>();
		capture.SetChecked(Profiling::ProfilingModule::Get().GetSnapshot().mEnabled);
		Label(capture, "Capture");
		capture.OnChanged = [](bool enabled) {
			Profiling::ProfilingModule::Get().SetEnabled(enabled);
		};
		Action(controls, "Clear history", []() {
			Profiling::ProfilingModule::Get().ClearHistory();
		});
		auto& diagnosticScroll = diagnostics.Add<UI::ScrollBox>();
		Fill(diagnostics, diagnosticScroll);
		auto& diagnosticColumn = diagnosticScroll.Add<UI::VerticalBox>();
		diagnosticColumn.SetSpacing(12);
		pDiagnostics = &Label(diagnosticColumn, "Capture is off.", 13);
		pDiagnostics->SetWrap(true);
		Label(diagnosticColumn, "SESSION LOG / latest 16 KiB", 12);
		pLogText = &Label(diagnosticColumn, "", 12);
		pLogText->SetWrap(true);
		pWorkspace->Add("performance", "Performance", std::move(diagnosticsWidget), EditorDockRegion::BottomRight);
		pStatus = &Label(shell, mNotice, 13);

		auto openSettings = [this]() {
			if (pWorkspace->Contains("editor-settings"))
			{
				pWorkspace->Show("editor-settings");
				return;
			}
			pWorkspace->Add("editor-settings", EditorSettingsDocument::GetTitle(), EditorSettingsDocument::Build(pWorkspace->GetColorPicker()),
				EditorDockRegion::Documents, true);
		};

		EditorSettings::Get().OnAccentColorChanged = [this](const UI::Color&) {
			mHierarchyDirty = mInspectorDirty = true;
		};

		pTitleBar->AddMenu("File", {{"New project", newProject.OnClicked}, {"New scene", newScene.OnClicked},
			{"Open map...", [this]() { mProjectAction = ProjectAction::OpenMap; }},
			{"Save map", [this]() { mProjectAction = ProjectAction::SaveMap; }},
			{"Save map as...", [this]() { mProjectAction = ProjectAction::SaveMapAs; }},
			{"Open project...", [this]() { mProjectAction = ProjectAction::Open; }},
			{"Save project", [this]() { mProjectAction = ProjectAction::Save; }},
			{"Save project as...", [this]() { mProjectAction = ProjectAction::SaveAs; }}, {"Exit", []() { Core::Engine::Get().GetMainWindow()->RequestClose(); }}});
		pTitleBar->AddMenu("Edit", {{"Undo", {}}, {"Redo", {}},
			{"Delete selected", deleteSelected.OnClicked},
			{"Editor Settings...", openSettings}});
		pTitleBar->SetMenuProvider("Edit", [this, remove = deleteSelected.OnClicked, openSettings]() {
			auto& registry = Core::Scene::Get().GetRegistry();
			const bool canCopy = mSelected != mCamera && registry.valid(mSelected) &&
				registry.all_of<Components::EntityInfoComponent>(mSelected);
			return EditorMenuPopup::Items{
				{"Undo", mHistory.CanUndo() ? std::function<void()>([this]() { ReplayHistory(false); }) : nullptr},
				{"Redo", mHistory.CanRedo() ? std::function<void()>([this]() { ReplayHistory(true); }) : nullptr},
				{"Duplicate", canCopy ? std::function<void()>([this]() { DuplicateEntity(mSelected); }) : nullptr},
				{"Copy", canCopy ? std::function<void()>([this]() { CopyEntity(mSelected); }) : nullptr},
				{"Paste", mCopiedEntity ? std::function<void()>([this]() { PasteEntity(*mCopiedEntity); }) : nullptr},
				{"Delete selected", remove},
				{"Editor Settings...", openSettings}};
		});
		pTitleBar->AddMenu("View", {{"Main viewport", [this]() { pWorkspace->Show("viewport"); }},
			{"Scene settings", [this]() { pWorkspace->Show("scene-settings"); }},
			{"Frame selected", frameSelected.OnClicked}, {"Reset layout", [this]() { pWorkspace->ResetLayout(); }}});
		pTitleBar->AddMenu("Project", {{"New project", newProject.OnClicked},
			{"Asset Browser", [this]() { pWorkspace->Show("assets"); }},
			{"Use selected camera for Play", [this]() {
				auto& reg = Core::Scene::Get().GetRegistry();
				if (mSelected != mCamera && reg.valid(mSelected) && reg.all_of<Components::CameraComponent>(mSelected))
				{
					mGameCamera = mSelected;
					mNotice = "Game camera: " + reg.get<Components::EntityInfoComponent>(mSelected).mName;
				}
				else mNotice = "Select an entity with a Camera component";
			}},
			{"Build Windows game...", [this]() { mProjectAction = ProjectAction::Export; }},
			{"Save material preset...", [this]() { mProjectAction = ProjectAction::SaveSurface; }},
			{"Apply material preset...", [this]() { mProjectAction = ProjectAction::LoadSurface; }},
			{"Save selected as prefab...", [this]() { mProjectAction = ProjectAction::SavePrefab; }},
			{"Place prefab...", [this]() { mProjectAction = ProjectAction::PlacePrefab; }},
			{"Link saved level...", [this]() { mProjectAction = ProjectAction::AddLevel; }},
			{"Choose C# assembly...", [this]() { mProjectAction = ProjectAction::Assembly; }},
			{"Use bundled C# behaviors", [this]() { mScriptAssembly.clear(); mNotice = "Using bundled C# behaviors"; }}});
		pTitleBar->AddMenu("Window", {{"Manage panels...", [this]() { pWorkspace->ToggleWindowMenu(); }},
			{"Scene settings", [this]() { pWorkspace->Show("scene-settings"); }},
			{"Editor Settings...", openSettings},
			{"Visual Script", [this]() {
				if (pWorkspace->Contains("visual-script"))
					pWorkspace->Show("visual-script");
				else
					pWorkspace->Add("visual-script", "Visual Script", std::make_unique<VisualScriptDocument>(),
						EditorDockRegion::Documents, true);
			}},
			{"Reset layout", [this]() { pWorkspace->ResetLayout(); }}});
		pTitleBar->AddMenu("Help", {{"About Nuclear Engine", [this]() {
			if (pWorkspace->Contains("about"))
			{
				pWorkspace->Show("about");
				return;
			}
			pWorkspace->Add("about", AboutDocument::GetTitle(), AboutDocument::Build(),
				EditorDockRegion::Documents, true);
		}}});
		mRoot = root->GetId();
		UI::UIModule::Get().GetContext().AddToViewport(std::move(root));
	}

	void EditorUI::CloseAddMenu()
	{
		if (mAddMenu)
			UI::UIModule::Get().GetContext().RemoveFromViewport(std::exchange(mAddMenu, 0));
	}

	void EditorUI::OpenAddMenu(UI::Rect anchor)
	{
		CloseAddMenu();
		pTitleBar->CloseMenu();
		auto& context = UI::UIModule::Get().GetContext();
		auto menu = std::make_unique<EditorAddMenu>(anchor, [this](int shape) {
			if (shape == 4)
				mPendingSkybox = true;
			else
				CreateEntity(shape);
		}, [this]() { CloseAddMenu(); });
		mAddMenu = menu->GetId();
		context.AddToViewport(std::move(menu), 60);
		context.SetFocus(mAddMenu);
	}

	bool EditorUI::PrepareSkybox()
	{
		if (pSkybox) return true;
		auto* camera = Core::Scene::Get().GetMainCamera();
		auto sky = std::make_unique<EditorSkybox>();
		if (!camera || !sky->Initialize(*camera)) return false;
		pSkybox = std::move(sky);
		return true;
	}

	void EditorUI::CreateSkybox()
	{
		auto& scene = Core::Scene::Get();
		entt::entity selected = entt::null;
		for (auto entity : scene.GetRegistry().view<SkyboxComponent>())
		{
			selected = entity;
			break;
		}
		if (selected == entt::null)
		{
			if (!PrepareSkybox())
			{
				mNotice = "Skybox could not be created";
				return;
			}
			selected = scene.CreateEntity("Skybox").GetEntityID();
			scene.GetRegistry().emplace<SkyboxComponent>(selected);
			mHistory.RecordCreated(selected);
		}
		mSelected = selected;
		mHierarchyDirty = mInspectorDirty = true;
		pInspector->SetOffset(0);
		pWorkspace->Show("viewport");
		mNotice = "Skybox selected";
	}

	void EditorUI::RefreshHierarchy()
	{
		auto column = std::make_unique<UI::VerticalBox>();
		column->SetSpacing(3);
		auto& registry = Core::Scene::Get().GetRegistry();
		std::vector<entt::entity> entities;
		for (auto entity : registry.view<Components::EntityInfoComponent>())
			if (entity != mCamera && registry.get<Components::EntityInfoComponent>(entity).mName.find(
										 mFilter) != std::string::npos)
				entities.push_back(entity);
		std::sort(entities.begin(), entities.end(), [](auto a, auto b) {
			return static_cast<Uint32>(a) < static_cast<Uint32>(b);
		});
		UI::Widget* selectedRow = nullptr;
		for (auto entity : entities)
		{
			auto& info = registry.get<Components::EntityInfoComponent>(entity);
			auto& row = column->Add<EditorHierarchyRow>();
			Label(row, (entity == mSelected ? "> " : "  ") + info.mName);
			row.OnClicked = [this, entity]() {
				mSelected = entity;
				pInspector->SetOffset(0);
				mHierarchyDirty = mInspectorDirty = true;
			};
			row.OnContextRequested = [this, entity](UI::UIContext& context, UI::Vec2 point) {
				OpenHierarchyMenu(entity, context, point);
			};
			//row.SetPadding({ 0, 0, 0, 0 });
			if (entity == mSelected)
			{
				selectedRow = &row;
				UI::ControlStyle style;
				style.Normal = {.24f, .20f, .045f, 1};
				style.Hovered = {.32f, .27f, .06f, 1};
				style.Pressed = {.4f, .32f, .06f, 1};
				style.Focus = {1, .8f, 0, 1};
				row.SetStyle(style);
			}
		}
		if (entities.empty())
			Label(*column, "No matching entities.", 13);
		pHierarchy->SetContent(std::move(column));
		if (mRevealSelection && selectedRow && pHierarchy->GetContext() && pHierarchy->IsVisible())
		{
			auto* context = pHierarchy->GetContext();
			context->Layout();
			pHierarchy->ScrollIntoView(*selectedRow);
			mRevealSelection = false;
		}
		if (!selectedRow)
			mRevealSelection = false;
		mHierarchyDirty = false;
	}

	void EditorUI::PickEntity()
	{
		const auto position = std::exchange(mPendingPick, std::nullopt);
		if (!position || mModal || !pViewport->IsVisible() || !pViewport->IsEnabled() ||
			pViewport->GetGizmoMode() != GizmoMode::Select)
			return;

		auto* context = pViewport->GetContext();
		context->Layout();
		const auto bounds = pViewport->GetBounds();
		if (bounds.x != mPickBounds.x || bounds.y != mPickBounds.y ||
			bounds.Width != mPickBounds.Width || bounds.Height != mPickBounds.Height)
			return;
		auto* camera = Core::Scene::Get().GetMainCamera();
		if (!camera)
			return;

		mSelected = pSelectionRenderer->Pick(*camera, bounds, *position);
		auto& registry = Core::Scene::Get().GetRegistry();
		auto* info = registry.valid(mSelected) ? registry.try_get<Components::EntityInfoComponent>(mSelected) : nullptr;
		if (info)
		{
			if (info->mName.find(mFilter) == std::string::npos)
			{
				mFilter.clear();
				pHierarchyFilter->SetText("");
			}
			mRevealSelection = true;
		}
		pInspector->SetOffset(0);
		mHierarchyDirty = mInspectorDirty = true;
	}

	void EditorUI::RefreshInspector()
	{
		pInspector->SetContent(mInspector.Build(mSelected, [this](bool structural) {
			auto& scene = Core::Scene::Get();
			scene.SetMainCamera(scene.GetRegistry().try_get<Components::CameraComponent>(mCamera));
			mHierarchyDirty = mInspectorDirty = true;
			mRendererDirty = mRendererDirty || structural;
		}, mHistory, pWorkspace->GetColorPicker()));
		mInspectorDirty = false;
	}

	void EditorUI::UpdateDiagnostics()
	{
		auto snapshot = Profiling::ProfilingModule::Get().GetSnapshot();
		const auto& counters = snapshot.mCounters;
		std::ostringstream text;
		text << std::fixed << std::setprecision(2);
		text << snapshot.mCpu.mName << "\nCapture / " << (snapshot.mEnabled ? "on" : "off") << "\n";
		if (!snapshot.mFrames.empty())
		{
			const auto& frame = snapshot.mFrames.back();
			text << "CPU frame / " << frame.mCpuMilliseconds << " ms\nGPU frame / ";
			if (frame.mGpuValid)
				text << frame.mGpuMilliseconds << " ms\n";
			else
				text << "pending or unavailable\n";
			text << "CPU scopes (inclusive)\n";
			for (const auto& scope : frame.mCpuTimings)
				text << scope.mName << " / " << scope.mTotalMilliseconds << " ms (" << scope.mCalls << ")\n";
			if (frame.mPipelineValid)
				text << "Input vertices / " << frame.mPipeline.mInputVertices << "\nInput primitives / "
					 << frame.mPipeline.mInputPrimitives << "\nVS invocations / "
					 << frame.mPipeline.mVertexInvocations << "\nPS invocations / "
					 << frame.mPipeline.mPixelInvocations << "\n";
			else
				text << "Pipeline statistics / pending or unavailable\n";
		}
		else
			text << "No captured frames\n";
		text << "Process CPU / ";
		if (counters.mProcessCpuValid)
			text << counters.mProcessCpuPercent << "%\n";
		else
			text << "pending or unavailable\n";
		if (counters.mMemoryValid)
			text << "Working set / " << counters.mMemory.mWorkingSetBytes / 1048576.0
				 << " MiB\nPrivate commit / " << counters.mMemory.mPrivateCommittedBytes / 1048576.0
				 << " MiB\n";
		else
			text << "Process memory / unavailable\n";
		if (counters.mGpuMemoryValid)
			text << "GPU local / " << counters.mGpuMemory.mLocalUsageBytes / 1048576.0 << " MiB / budget "
				 << counters.mGpuMemory.mLocalBudgetBytes / 1048576.0 << " MiB\n";
		else
			text << "GPU memory / unavailable\n";
		auto memory = Profiling::MemoryProfiler::Get().GetSnapshot();
		text << "Explicit accounting / " << memory.mLiveBytes << " bytes in " << memory.mLiveAllocations
			 << " allocations\n";
		for (const auto& category : memory.mCategories)
			text << category.mName << " / " << category.mLiveBytes << " bytes\n";
		text << "Dropped CPU / " << snapshot.mDroppedCpuScopes << " / GPU " << snapshot.mDroppedGpuQueries
			 << " / invalid GPU " << snapshot.mInvalidGpuResults;
		pDiagnostics->SetText(text.str());
		pLogText->SetText(mLogs.Read());
	}

	void EditorUI::Update(float deltaTime)
	{
		UpdateProjectActions();
		pTitleBar->UpdateMenu();
		auto& context = UI::UIModule::Get().GetContext();
		if (mModal && !context.Find(mModal))
		{
			mModal = 0;
			if (auto* shell = context.Find(mRoot))
				shell->SetEnabled(true);
		}
		pWorkspace->SetInteractionEnabled(mModal == 0);
		if (mModal)
			mHistoryModifiers.fill(false);
		auto documents = std::exchange(mPendingDocuments, {});
		for (const auto& [type, id] : documents)
			OpenAssetDocument(type, id);
		pWorkspace->Update();
		EditorAssetPicker::Update(context);
		for (const auto& window : pWorkspace->GetWindows()) EditorAssetPicker::Update(window->GetContext());
		if (mHierarchyMenu)
		{
			auto* menuContext = GetHierarchyMenuContext();
			auto& registry = Core::Scene::Get().GetRegistry();
			if (!menuContext || !menuContext->Find(mHierarchyMenu) || mModal ||
				!registry.valid(mHierarchyMenuEntity) || pHierarchy->GetContext() != menuContext ||
				!pHierarchy->IsVisible() || !pHierarchy->IsEnabled()) CloseHierarchyMenu();
		}
		context.Layout();
		for (const auto& window : pWorkspace->GetWindows()) window->GetContext().Layout();
		UpdateMeshPlacement();
		pSceneToolbar->SetVisibility(pWorkspace->GetDocumentLayout("viewport")->IsVisible()
			? UI::Visibility::Visible : UI::Visibility::Collapsed);
		if (mAddMenu)
		{
			bool focused = false;
			for (auto* widget = context.Find(context.GetFocusedWidget()); widget; widget = widget->GetParent())
				focused = focused || widget->GetId() == mAddMenu;
			if (!focused || !pSceneToolbar->IsVisible() || !pSceneToolbar->IsEnabled())
				CloseAddMenu();
		}
		for (const auto& window : pWorkspace->GetWindows())
		{
			auto& windowContext = window->GetContext();
			windowContext.OnPreviewEvent = [this, &windowContext](const Platform::Event& event) {
				return ProcessShortcut(event, windowContext);
			};
		}
		auto* importOwner = Core::Engine::Get().GetMainWindow();
		for (const auto& window : pWorkspace->GetWindows())
			if (window->GetWindow().HasFocus())
				importOwner = &window->GetWindow();
		mAssets.Update(*importOwner);
		if (std::exchange(mPendingSkybox, false))
			CreateSkybox();
		if (mSceneSettingsDirty)
		{
			pSceneSettings->Refresh();
			mSceneSettingsDirty = false;
		}
		// Authoring camera additions/removals can move EnTT's packed camera storage.
		auto& scene = Core::Scene::Get();
		scene.SetMainCamera(scene.GetRegistry().try_get<Components::CameraComponent>(mCamera));
		PickEntity();
		pViewport->SetSelectedEntity(mSelected);
		pViewport->UpdateCamera(deltaTime);
		for (size_t mode = 0; mode < ViewportModes.size(); ++mode)
		{
			UI::ControlStyle style;
			style.Normal = pViewport->GetGizmoMode() == ViewportModes[mode]
							   ? UI::Color{.035f, .28f, .25f, 1}
							   : UI::Color{.1f, .13f, .16f, 1};
			style.Hovered = {.075f, .3f, .3f, 1};
			style.Pressed = {.03f, .24f, .24f, 1};
			style.Focus = {.15f, .7f, .6f, 1};
			mGizmoButtons[mode]->SetStyle(style);
		}
		if (mRendererDirty)
			RebuildRenderer();
		if (mHierarchyDirty || (mRevealSelection && pHierarchy->GetContext() && pHierarchy->IsVisible()))
			RefreshHierarchy();
		if (mInspectorDirty)
			RefreshInspector();
		if (mAssetsDirty)
		{
			auto root = mProjectFile.empty() ? mStartupMap.parent_path() : mProjectFile.parent_path();
			if (root.empty()) root = std::filesystem::absolute(Assets::AssetLibrary::Get().GetPath());
			std::vector<std::filesystem::path> maps{mStartupMap, mMapFile};
			for (const auto& [name, path] : mLevels) maps.push_back(path);
			pAssets->SetContent(mAssets.Build(root, maps));
			mAssetsDirty = false;
		}
		pStatus->SetText(mProject.GetProjectInfo().mProjectName + " / " + mNotice);
		pTitleBar->SetTitle(mProject.GetProjectInfo().mProjectName + " - Nuclear Editor");
		pSceneTitle->SetText(Core::Scene::Get().GetSceneAsset()->GetName());
		mDiagnosticTimer -= deltaTime;
		if (mDiagnosticTimer <= 0)
		{
			UpdateDiagnostics();
			mDiagnosticTimer = .5f;
		}
	}

	void EditorUI::Render(float deltaTime)
	{
		for (const auto& window : pWorkspace->GetWindows()) window->GetContext().Layout();
		auto& context = UI::UIModule::Get().GetContext();
		context.Layout();
		for (auto& [key, preview] : mMeshPreviews)
			if (preview)
			{
				preview->Update(deltaTime);
				preview->Render();
			}
		auto bounds = pViewport->GetBounds();
		auto* camera = Core::Scene::Get().GetMainCamera();
		const float viewportScale = pViewport->GetContext() ? pViewport->GetContext()->GetScale() : context.GetScale();
		const auto width = static_cast<Uint32>(std::max(1.f, bounds.Width * viewportScale));
		const auto height = static_cast<Uint32>(std::max(1.f, bounds.Height * viewportScale));
		if (pViewport->IsVisible() && camera->GetColorRT().GetDimensions() != Math::Vector2ui(width, height))
		{
			camera->ResizeRTs(width, height);
			camera->SetProjectionMatrix(
				Math::perspective(Math::radians(45.f), float(width) / float(height), .1f, 1000.f));
		}
		camera->UpdateBuffer();
		Diligent::Viewport viewport;
		viewport.Width = static_cast<float>(width);
		viewport.Height = static_cast<float>(height);
		Graphics::GraphicsModule::Get().GetContext()->SetViewports(1, &viewport, width, height);
		if (pViewport->IsVisible())
		{
			pRenderer->Update(deltaTime);
			for (auto entity : Core::Scene::Get().GetRegistry().view<SkyboxComponent>())
				if (pSkybox && Core::Scene::Get().GetRegistry().get<SkyboxComponent>(entity).Enabled)
				{
					pSkybox->Render(*camera);
					break;
				}
			pSelectionRenderer->Render(*camera, mSelected, viewportScale);
			if (pMeshPlacement && !mModal) pMeshPlacement->Render(*camera);
		}
		auto& graphics = Graphics::GraphicsModule::Get();
		auto* target = graphics.GetSwapChain()->GetCurrentBackBufferRTV();
		graphics.GetContext()->SetRenderTargets(1, &target, nullptr,
												Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		const float clear[] = {.035f, .045f, .06f, 1};
		graphics.GetContext()->ClearRenderTarget(target, clear,
												 Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		pWorkspace->RenderWindows();
	}

	void EditorUI::Resize(int width, int height)
	{
		const float scale = Core::Engine::Get().GetMainWindow()->GetDpiScale();
		UI::UIModule::Get().GetContext().SetViewport(float(width), float(height), scale);
	}

	void EditorUI::Shutdown()
	{
		FinishPlay();
		CloseHierarchyMenu();
		mCopiedEntity.reset();
		CancelMeshPlacement();
		mInspector.Reset();
		CloseAddMenu();
		mPendingSkybox = false;
		mHistory.Clear();
		mHistoryModifiers.fill(false);
		mPendingPick.reset();
		mAssetHeadings.clear();
		mMeshPreviews.clear();
		pWorkspace.reset();
		mPendingDocuments.clear();
		UI::UIModule::Get().GetContext().Clear();
		pRenderer.reset();
		pSkybox.reset();
		pSelectionRenderer.reset();
		Core::Scene::Get().SetMainCamera(static_cast<Components::CameraComponent*>(nullptr));
		Core::Scene::Get().Clear();
		mRoot = mModal = 0;
		pMeshPlacement.reset();
		mAssets.Shutdown();
		pHierarchy = nullptr;
		pHierarchyFilter = nullptr;
		pInspector = nullptr;
		pAssets = nullptr;
		pViewport = nullptr;
		pSceneToolbar = nullptr;
		pSceneSettings = nullptr;
		pTitleBar = nullptr;
		mGizmoButtons.fill(nullptr);
		pDiagnostics = pLogText = pStatus = pSceneTitle = nullptr;
	}

	void EditorUI::OnAuxiliaryWindowEvent(const Platform::Event& event)
	{
		if (pWorkspace)
			pWorkspace->ProcessEvent(event);
	}

	void EditorUI::ReplayHistory(bool redo)
	{
		if (mModal)
			return;
		CloseHierarchyMenu();
		if (pAssets && pAssets->GetContext()) pAssets->GetContext()->CancelSubtree(*pAssets);
		CancelMeshPlacement();
		pViewport->CancelInteraction();
		pTitleBar->CloseMenu();
		const bool restored = redo ? mHistory.Redo() : mHistory.Undo();
		mNotice = restored ? (redo ? "Redo" : "Undo") : "No history action available";
	}

	bool EditorUI::ProcessShortcut(const Platform::Event& event, UI::UIContext& context)
	{
		if (EditorAssetPicker::ProcessEvent(context, event)) return true;
		using Key = Platform::Input;
		if (event.mType == Platform::EventType::FocusChanged)
			mHistoryModifiers.fill(false);
		if (event.mType != Platform::EventType::Key)
			return false;
		const int modifiers[] = {Key::KEYCODE_LCTRL, Key::KEYCODE_RCTRL, Key::KEYCODE_LSHIFT, Key::KEYCODE_RSHIFT};
		for (size_t index = 0; index < mHistoryModifiers.size(); ++index)
			if (event.mKey == modifiers[index])
				mHistoryModifiers[index] = event.mPressed;
		const bool control = mHistoryModifiers[0] || mHistoryModifiers[1];
		const bool shift = mHistoryModifiers[2] || mHistoryModifiers[3];
		if (!control || !event.mPressed || (event.mKey != Key::KEYCODE_Z && event.mKey != Key::KEYCODE_Y &&
			event.mKey != Key::KEYCODE_C && event.mKey != Key::KEYCODE_V))
			return false;
		// The graph owns its editing surface even when a toolbar button has focus.
		auto* graph = pWorkspace ? pWorkspace->GetDocumentLayout("visual-script") : nullptr;
		if (graph && graph->GetContext() == &context && graph->IsVisible()) return false;
		if (mModal || context.GetInputMode() == UI::InputMode::GameOnly)
			return false;
		for (auto* focus = context.Find(context.GetFocusedWidget()); focus; focus = focus->GetParent())
			if (dynamic_cast<EditorAssetPicker*>(focus)) return false;
		// Text boxes hold uncommitted text. Scene shortcuts belong to the workspace outside text editing.
		if (dynamic_cast<UI::TextBox*>(context.Find(context.GetFocusedWidget())))
			return false;
		if (!event.mRepeat)
		{
			if (event.mKey == Key::KEYCODE_C) CopyEntity(mSelected);
			else if (event.mKey == Key::KEYCODE_V)
			{
				if (mCopiedEntity) PasteEntity(*mCopiedEntity);
			}
			else ReplayHistory(event.mKey == Key::KEYCODE_Y || shift);
		}
		return true;
	}

	void EditorUI::OpenAssetDocument(Assets::AssetType type, const Core::UUID& id)
	{
		const auto key = "asset_" + std::to_string(static_cast<int>(type)) + "_" + id.str();
		auto* asset = AssetDocument::Resolve(type, id);
		if (!asset)
			return;
		const auto title = AssetDocument::GetTitle(type, asset->GetName());
		if (pWorkspace->Contains(key))
		{
			pWorkspace->SetTitle(key, title);
			pWorkspace->Show(key);
			return;
		}
		auto content = AssetDocument::Build(type, id, [this, id]() {
			auto* mesh = Assets::AssetLibrary::Get().mImportedMeshes.GetAsset(id);
			if (!mesh || mesh->GetState() != Assets::IAsset::State::Created)
				return;
			auto entity = Core::Scene::Get().CreateEntity(mesh->GetName());
			entity.AddComponent<Components::MeshComponent>(mesh, mProject.GetMeshMaterial(*mesh));
			mSelected = entity.GetEntityID();
			mHistory.RecordCreated(mSelected);
			mHierarchyDirty = mInspectorDirty = true;
			pInspector->SetOffset(0);
			pWorkspace->Show("viewport");
		}, &mAssetHeadings[key], &mMeshPreviews[key]);
		pWorkspace->Add(key, title, std::move(content), EditorDockRegion::Documents, true);
	}
} // namespace Nuclear::Editor
