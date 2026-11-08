#include <EditorUI.h>
#include <EditorTitleBar.h>
#include <SceneSettings.h>
#include <EditorAddMenu.h>
#include <EditorSkybox.h>
#include <SkyboxComponent.h>
#include <EditorWidgets.h>
#include <AssetDocument.h>
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
		Assets::ShaderImportingDesc desc;
		desc.mCommonOptions.mAsyncImport = false;
		desc.mCommonOptions.mLoadOnly = true;
		desc.mExcludedVariants = {"NE_DEFFERED", "NE_SHADOWS", "NE_ANIMATION"};
		pShader = Assets::AssetManager::Get().Import<Assets::Shader>(
			"@NuclearAssets@/Shaders/BlinnPhong.NuclearShader", desc);
		if (!pShader || pShader->GetState() != Assets::IAsset::State::Loaded)
			throw std::runtime_error("Editor preview shader could not be imported");
		auto& material = Assets::AssetLibrary::Get().mImportedMaterials.AddAsset();
		material.SetName("Editor / Default material");
		pMaterial = &material;
		mProject.Initalize({"Untitled Project"});
		mProject.SetDefaultMaterial(pMaterial);
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
		mAssets.OnAssetOpened = [this](Assets::AssetType type, Core::UUID id) {
			mPendingDocuments.emplace_back(type, id);
		};
		NewScene();
		BuildWorkspace();
		CreateEntity(1);
		mHistory.Clear();
		auto& context = UI::UIModule::Get().GetContext();
		const auto size = context.GetPixelSize();
		Resize(static_cast<int>(size.x), static_cast<int>(size.y));
		context.SetInputMode(UI::InputMode::UIOnly);
		context.OnPreviewEvent = [this, &context](const Platform::Event& event) {
			return ProcessShortcut(event, context);
		};
		Platform::Input::Get().SetMouseInputMode(Platform::Input::MouseInputMode::Normal);
	}

	void EditorUI::NewScene()
	{
		CloseAddMenu();
		mPendingSkybox = false;
		if (pViewport)
			pViewport->SetSelectedEntity(entt::null);
		mHistory.Clear();
		pRenderer.reset();
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
		mSelected = entt::null;
		RebuildRenderer();
		mHierarchyDirty = mInspectorDirty = mAssetsDirty = true;
		mNotice = "New scene / session changes are held in memory";
		if (pWorkspace)
			pWorkspace->Show("viewport");
	}

	void EditorUI::RebuildRenderer()
	{
		pRenderer = std::make_unique<Systems::RenderSystem>();
		pRenderer->RegisterShader(pShader);
		Systems::RenderSystemBakingDesc desc;
		desc.RTWidth = 800;
		desc.RTHeight = 600;
		desc.mRenderToScreen = false;
		pRenderer->Bake(desc);
		pRenderer->AddRenderPass(&mGeometry);
		if (pMaterial->GetTextures().empty())
		{
			Assets::MaterialTextureSet set;
			set.mData.push_back(Fallbacks::FallbacksModule::Get().GetDefaultDiffuseTex());
			pMaterial->GetTextures().push_back(set);
		}
		if (pMaterial->GetState() != Assets::IAsset::State::Created)
			pMaterial->Create(pShader);
		for (auto entity : Core::Scene::Get().GetRegistry().view<Components::MeshComponent>())
		{
			auto& mesh = Core::Scene::Get().GetRegistry().get<Components::MeshComponent>(entity);
			mesh.SetMaterial(mesh.GetMaterial());
		}
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
			Label(form, "Creates an in-memory project and replaces the current scene.").SetWrap(true);
			auto& name = form.Add<UI::TextBox>("New Project");
			const auto id = modal->GetId();
			mModal = id;
			auto& row = form.Add<UI::HorizontalBox>();
			row.SetSpacing(10);
			Action(row, "Create project", [this, &name, id]() {
				mProject.Initalize({name.GetText().empty() ? "New Project" : name.GetText()});
				NewScene();
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
			NewScene();
		});
		auto& add = Action(actions, "Add", {});
		add.OnClicked = [this, &add]() { OpenAddMenu(add.GetBounds()); };
		auto& vsync = actions.Add<UI::CheckBox>();
		vsync.SetChecked(true);
		Label(vsync, "VSync");
		vsync.OnChanged = [](bool enabled) {
			Core::Engine::Get().SetSwapChainSyncInterval(enabled ? 1 : 0);
		};
		auto& documentHost = shell.Add<EditorDockArea>();
		Fill(shell, documentHost);
		pWorkspace = std::make_unique<EditorWorkspace>(UI::UIModule::Get().GetContext(), documents, documentHost);

		auto hierarchyWidget = std::make_unique<UI::VerticalBox>();
		auto& hierarchy = *hierarchyWidget;
		hierarchy.SetPadding({12, 10, 12, 10});
		hierarchy.SetSpacing(8);
		auto& filter = hierarchy.Add<UI::TextBox>();
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
		auto& viewActions = scene.Add<UI::HorizontalBox>();
		viewActions.SetSpacing(8);
		const char* modes[] = {"Select", "Move", "Rotate", "Scale"};
		for (size_t mode = 0; mode < ViewportModes.size(); ++mode)
			mGizmoButtons[mode] = &Action(viewActions, modes[mode], [this, mode]() {
				pViewport->SetGizmoMode(ViewportModes[mode]);
			});
		auto& frameSelected = Action(viewActions, "Frame selected", [this]() {
			auto& scene = Core::Scene::Get();
			if (auto* info = scene.GetRegistry().try_get<Components::EntityInfoComponent>(mSelected))
			{
				auto* camera = scene.GetMainCamera();
				camera->SetPosition(info->mTransform.GetLocalPosition() - camera->GetFrontView() * 6.0f);
			}
		});
		auto& deleteSelected = Action(viewActions, "Delete selected", [this]() {
			auto& registry = Core::Scene::Get().GetRegistry();
			if (registry.valid(mSelected) && mSelected != mCamera)
			{
				pViewport->SetSelectedEntity(entt::null);
				mHistory.Edit(mSelected, "Delete entity", [&]() { registry.destroy(mSelected); });
				Core::Scene::Get().SetMainCamera(registry.try_get<Components::CameraComponent>(mCamera));
				mSelected = entt::null;
				mHierarchyDirty = mInspectorDirty = mRendererDirty = true;
			}
		});
		pViewport = &scene.Add<GameViewPort>();
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
		});
		pSceneSettings = settings.get();
		pWorkspace->Add("scene-settings", "Scene settings", std::move(settings), EditorDockRegion::Right);
		pWorkspace->Close("scene-settings");

		auto assetsWidget = std::make_unique<UI::VerticalBox>();
		auto& assets = *assetsWidget;
		assets.SetPadding({12, 10, 12, 10});
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
		pTitleBar->AddMenu("File", {{"New project", newProject.OnClicked}, {"New scene", newScene.OnClicked},
			{"Save scene (unavailable)", {}}, {"Exit", []() { Core::Engine::Get().GetMainWindow()->RequestClose(); }}});
		pTitleBar->AddMenu("Edit", {{"Undo", {}}, {"Redo", {}},
			{"Delete selected", deleteSelected.OnClicked}});
		pTitleBar->SetMenuProvider("Edit", [this, remove = deleteSelected.OnClicked]() {
			return EditorMenuPopup::Items{
				{"Undo", mHistory.CanUndo() ? std::function<void()>([this]() { ReplayHistory(false); }) : nullptr},
				{"Redo", mHistory.CanRedo() ? std::function<void()>([this]() { ReplayHistory(true); }) : nullptr},
				{"Delete selected", remove}};
		});
		pTitleBar->AddMenu("View", {{"Main viewport", [this]() { pWorkspace->Show("viewport"); }},
			{"Scene settings", [this]() { pWorkspace->Show("scene-settings"); }},
			{"Frame selected", frameSelected.OnClicked}, {"Reset layout", [this]() { pWorkspace->ResetLayout(); }}});
		pTitleBar->AddMenu("Project", {{"New project", newProject.OnClicked},
			{"Asset Browser", [this]() { pWorkspace->Show("assets"); }},
			{"Project settings (unavailable)", {}}});
		pTitleBar->AddMenu("Window", {{"Manage panels...", [this]() { pWorkspace->ToggleWindowMenu(); }},
			{"Scene settings", [this]() { pWorkspace->Show("scene-settings"); }},
			{"Reset layout", [this]() { pWorkspace->ResetLayout(); }}});
		pTitleBar->AddMenu("Help", {{"About Nuclear Engine", [this]() { mNotice = "Nuclear Engine / session editor"; }}});
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
			if (!pSkybox)
			{
				auto sky = std::make_unique<EditorSkybox>();
				if (!scene.GetMainCamera() || !sky->Initialize(*scene.GetMainCamera()))
				{
					mNotice = "Skybox could not be created";
					return;
				}
				pSkybox = std::move(sky);
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
		column->SetSpacing(6);
		auto& registry = Core::Scene::Get().GetRegistry();
		std::vector<entt::entity> entities;
		for (auto entity : registry.view<Components::EntityInfoComponent>())
			if (entity != mCamera && registry.get<Components::EntityInfoComponent>(entity).mName.find(
										 mFilter) != std::string::npos)
				entities.push_back(entity);
		std::sort(entities.begin(), entities.end(), [](auto a, auto b) {
			return static_cast<Uint32>(a) < static_cast<Uint32>(b);
		});
		for (auto entity : entities)
		{
			auto& info = registry.get<Components::EntityInfoComponent>(entity);
			Action(*column, (entity == mSelected ? "> " : "  ") + info.mName, [this, entity]() {
				mSelected = entity;
				pInspector->SetOffset(0);
				mHierarchyDirty = mInspectorDirty = true;
			});
		}
		if (entities.empty())
			Label(*column, "No matching entities.", 13);
		pHierarchy->SetContent(std::move(column));
		mHierarchyDirty = false;
	}

	void EditorUI::RefreshInspector()
	{
		pInspector->SetContent(mInspector.Build(mSelected, [this](bool structural) {
			auto& scene = Core::Scene::Get();
			scene.SetMainCamera(scene.GetRegistry().try_get<Components::CameraComponent>(mCamera));
			mHierarchyDirty = mInspectorDirty = true;
			mRendererDirty = mRendererDirty || structural;
		}, mHistory));
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
		mAssets.Update();
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
		if (mHierarchyDirty)
			RefreshHierarchy();
		if (mInspectorDirty)
			RefreshInspector();
		if (mAssetsDirty)
		{
			pAssets->SetContent(mAssets.Build());
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
		auto& context = UI::UIModule::Get().GetContext();
		context.Layout();
		auto bounds = pViewport->GetBounds();
		auto* camera = Core::Scene::Get().GetMainCamera();
		const auto width = static_cast<Uint32>(std::max(1.f, bounds.Width * context.GetScale()));
		const auto height = static_cast<Uint32>(std::max(1.f, bounds.Height * context.GetScale()));
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
		float scale = std::min({1.0f, width / 1440.0f, height / 900.0f});
		UI::UIModule::Get().GetContext().SetViewport(float(width), float(height), std::max(.1f, scale));
	}

	void EditorUI::Shutdown()
	{
		CloseAddMenu();
		mPendingSkybox = false;
		mHistory.Clear();
		mHistoryModifiers.fill(false);
		pWorkspace.reset();
		mPendingDocuments.clear();
		UI::UIModule::Get().GetContext().Clear();
		pRenderer.reset();
		pSkybox.reset();
		Core::Scene::Get().SetMainCamera(static_cast<Components::CameraComponent*>(nullptr));
		Core::Scene::Get().Clear();
		mRoot = mModal = 0;
		pHierarchy = nullptr;
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
		pViewport->CancelInteraction();
		pTitleBar->CloseMenu();
		const bool restored = redo ? mHistory.Redo() : mHistory.Undo();
		mNotice = restored ? (redo ? "Redo" : "Undo") : "No history action available";
	}

	bool EditorUI::ProcessShortcut(const Platform::Event& event, UI::UIContext& context)
	{
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
		if (!control || !event.mPressed || (event.mKey != Key::KEYCODE_Z && event.mKey != Key::KEYCODE_Y))
			return false;
		if (mModal || context.GetInputMode() == UI::InputMode::GameOnly)
			return false;
		// Text boxes hold uncommitted text. Scene shortcuts belong to the workspace outside text editing.
		if (dynamic_cast<UI::TextBox*>(context.Find(context.GetFocusedWidget())))
			return false;
		if (!event.mRepeat)
			ReplayHistory(event.mKey == Key::KEYCODE_Y || shift);
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
			entity.AddComponent<Components::MeshComponent>(mesh, pMaterial);
			mSelected = entity.GetEntityID();
			mHistory.RecordCreated(mSelected);
			mHierarchyDirty = mInspectorDirty = true;
			pInspector->SetOffset(0);
			pWorkspace->Show("viewport");
		});
		pWorkspace->Add(key, title, std::move(content), EditorDockRegion::Documents, true);
	}
} // namespace Nuclear::Editor
