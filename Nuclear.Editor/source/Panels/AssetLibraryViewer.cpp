#include <AssetLibraryViewer.h>
#include <EditorSettings.h>
#include <EditorWidgets.h>
#include <AssetDocument.h>
#include <EditorAssetItem.h>
#include <Nuclear.Platform/FileDialog.h>
#include <algorithm>
#include <cctype>
#include <Threading/ThreadingModule.h>
#include <Fallbacks/FallbacksModule.h>
namespace Nuclear::Editor
{
	Assets::IAsset* AssetLibraryViewer::ResolveAsset(Assets::AssetType type, const Core::UUID& id)
	{
		if (type == Assets::AssetType::Scene)
			return Assets::AssetLibrary::Get().mImportedScenes.GetAsset(id);
		return AssetDocument::Resolve(type, id);
	}

	void AssetLibraryViewer::AddToScene(const Core::UUID& id)
	{
		auto* mesh = Assets::AssetLibrary::Get().mImportedMeshes.GetAsset(id);
		if (!pProject || !mesh || mesh->GetState() != Assets::IAsset::State::Created)
			return;
		auto entity = Core::Scene::Get().CreateEntity(mesh->GetName());
		entity.AddComponent<Components::MeshComponent>(mesh, pProject->GetMeshMaterial(*mesh));
		if (OnEntityAdded)
			OnEntityAdded(entity.GetEntityID());
	}

	EditorMenuPopup::Items AssetLibraryViewer::AssetCommands(Assets::IAsset& asset)
	{
		const auto type = asset.GetType();
		const auto id = asset.GetUUID();
		std::function<void()> add;
		if (type == Assets::AssetType::Mesh && asset.GetState() == Assets::IAsset::State::Created)
			add = [this, id]() { AddToScene(id); };
		return {
			{"Add to scene", std::move(add)},
			{"Delete", [this, type, id]() { DeleteAsset(type, id); }},
			{"Rename", [this, type, id]() {
				if (auto* target = ResolveAsset(type, id))
				{
					mRenameType = type;
					mRenameId = id;
					mRenameText = target->GetName();
					mRenaming = true;
					if (OnChanged) OnChanged();
				}
			}}
		};
	}

	void AssetLibraryViewer::DeleteAsset(Assets::AssetType type, const Core::UUID& id)
	{
		// Transfer the node into a private map without moving or destroying the asset.
		// Reinsertion keeps existing scene/history/preview references usable under the node-handle contract.
		auto retire = [this, &id](auto& map) {
			auto retired = std::make_shared<decltype(map.mData)>();
			auto node = map.mData.extract(id);
			if (node.empty())
				return false;
			retired->insert(std::move(node));
			mRetiredAssets.push_back(std::move(retired));
			return true;
		};
		auto& library = Assets::AssetLibrary::Get();
		bool removed = false;
		switch (type)
		{
		case Assets::AssetType::Texture: removed = retire(library.mImportedTextures); break;
		case Assets::AssetType::Mesh: removed = retire(library.mImportedMeshes); break;
		case Assets::AssetType::Material: removed = retire(library.mImportedMaterials); break;
		case Assets::AssetType::Scene: removed = retire(library.mImportedScenes); break;
		default: break;
		}
		if (!removed) return;
		mRenaming = false;
		mStatus = "Asset removed from the session library. Source files are unchanged.";
		if (OnAssetChanged) OnAssetChanged(type, id, true);
		if (OnChanged) OnChanged();
	}

	void AssetLibraryViewer::RenameAsset(const std::string& name)
	{
		const auto first = name.find_first_not_of(" \t\r\n");
		if (first == std::string::npos)
		{
			mStatus = "Enter a nonblank asset name.";
			if (OnChanged) OnChanged();
			return;
		}
		if (auto* asset = ResolveAsset(mRenameType, mRenameId))
		{
			asset->SetName(name.substr(first, name.find_last_not_of(" \t\r\n") - first + 1));
			mStatus = "Asset renamed: " + asset->GetName();
			if (OnAssetChanged) OnAssetChanged(mRenameType, mRenameId, false);
		}
		else mStatus = "This asset is no longer available.";
		mRenaming = false;
		if (OnChanged) OnChanged();
	}

	void AssetLibraryViewer::SetProject(Project* project)
	{
		pProject = project;
	}

	void AssetLibraryViewer::Shutdown()
	{
		mMeshPreviewCache.Clear();
		mRetiredAssets.clear();
		mPendingImport.clear();
		mOpenImportDialog = mRenaming = false;
		pProject = nullptr;
	}

	void AssetLibraryViewer::Update(Platform::Window& dialogOwner)
	{
		if (std::exchange(mOpenImportDialog, false) && pProject)
		{
			std::string error;
			auto selected = Platform::FileDialog::OpenFile(dialogOwner, "Import asset",
				"*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.hdr;*.obj;*.fbx;*.dae;*.gltf;*.glb;*.NuclearShader;*.ttf",
				pProject->GetPath(), error);
			if (!selected.empty())
				mPendingImport = std::move(selected);
			else
			{
				mStatus = error.empty() ? "Import cancelled." : error;
				if (OnChanged)
					OnChanged();
			}
		}
		if (mPendingImport.empty())
			return;
		auto path = std::exchange(mPendingImport, {});
		std::string extension = path.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
			return char(std::tolower(c));
		});
		Assets::IAsset* result = nullptr;
		auto& manager = Assets::AssetManager::Get();
		try
		{
			if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" ||
				extension == ".tga" || extension == ".hdr")
			{
				Assets::TextureImportingDesc desc;
				desc.mCommonOptions.mAsyncImport = false;
				desc.mCommonOptions.mLoadOnly = true;
				result = manager.Import<Assets::Texture>(path.string(), desc);
				if (result == Fallbacks::FallbacksModule::Get().GetDefaultBlackImage())
					result = nullptr;
			}
			else if (extension == ".obj" || extension == ".fbx" || extension == ".dae" ||
					 extension == ".gltf" || extension == ".glb")
			{
				Assets::MeshImportingDesc desc;
				desc.mImportMaterial = true;
				desc.mSaveMaterialNames = true;
				desc.mImportAnimations = false;
				desc.mCommonOptions.mAsyncImport = false;
				desc.mCommonOptions.mLoadOnly = true;
				result = manager.Import<Assets::Mesh>(path.string(), desc);
				// Synchronous mesh decoding still queues its GPU creation on the main thread.
				auto& threading = Threading::ThreadingModule::Get();
				while (!threading.GetMainTasks().empty())
					threading.ExecuteMainThreadTasks(1);
				if (result && result->GetState() == Assets::IAsset::State::Created && OnMeshImported)
					OnMeshImported(*static_cast<Assets::Mesh*>(result));
			}
			else if (extension == ".nuclearshader")
			{
				Assets::ShaderImportingDesc desc;
				desc.mCommonOptions.mAsyncImport = false;
				desc.mCommonOptions.mLoadOnly = true;
				result = manager.Import<Assets::Shader>(path.string(), desc);
			}
			else if (extension == ".ttf")
			{
				Assets::FontImportingDesc desc;
				result = manager.Import<Assets::Font>(path.string(), desc);
			}
			else
			{
				mStatus = "Supported: textures, OBJ/FBX/DAE/glTF geometry, NuclearShader and TTF.";
				if (OnChanged)
					OnChanged();
				return;
			}
			mStatus = result && result->GetState() == Assets::IAsset::State::Created
						  ? "Resource created: " + path.filename().string()
						  : "Import did not create a ready resource. See NuclearEngine.log.";
		}
		catch (const std::exception& error)
		{
			mStatus = std::string("Import failed: ") + error.what();
		}
		if (OnChanged)
			OnChanged();
	}

	std::unique_ptr<UI::Widget> AssetLibraryViewer::Build()
	{
		auto root = std::make_unique<UI::VerticalBox>();
		root->SetSpacing(8);

		auto& tabs = root->Add<UI::HorizontalBox>();
		tabs.SetSpacing(6);

		const char* names[] = {"Files", "Textures", "Meshes", "Materials", "Scenes"};
		for (int i = 0; i < 5; ++i)
		{
			auto& button = Action(tabs, names[i], [this, i]() {
				mCategory = i;
				if (OnChanged)
					OnChanged();
			});
			button.SetPadding({6, 7, 6, 7});
			if (i == mCategory)
			{
				UI::ControlStyle activeStyle;
				activeStyle.Normal = {.12f, .17f, .22f, 1.0f};
				activeStyle.Hovered = {.16f, .22f, .28f, 1.0f};
				activeStyle.Pressed = {.09f, .13f, .17f, 1.0f};
				activeStyle.Focus = EditorSettings::Get().GetAccentColor();
				button.SetStyle(activeStyle);
			}
		}

		// Expanding spacer pushing the Import button to the right side of the tabs bar
		auto& spacer = tabs.Add<UI::HorizontalBox>();
		Fill(tabs, spacer, 1.0f);

		// Import button placed on the right side of the tabs bar
		auto& import = Action(tabs, "Import...", [this]() {
			mOpenImportDialog = true;
		});
		import.SetEnabled(pProject != nullptr);
		import.SetPadding({8, 7, 8, 7});
		auto& scroll = root->Add<UI::ScrollBox>();
		Fill(*root, scroll);
		auto& content = scroll.Add<UI::VerticalBox>();
		content.SetSpacing(6);
		Label(content, mStatus, 13).SetWrap(true);
		if (mRenaming)
		{
			auto& form = Section(content, "Rename asset");
			auto& name = form.Add<UI::TextBox>(mRenameText);
			name.OnChanged = [this](const std::string& value) { mRenameText = value; };
			name.OnCommitted = [this](const std::string& value) { RenameAsset(value); };
			auto& actions = form.Add<UI::HorizontalBox>();
			actions.SetSpacing(6);
			Action(actions, "Save name", [this]() { RenameAsset(mRenameText); });
			Action(actions, "Cancel rename", [this]() {
				mRenaming = false;
				if (OnChanged) OnChanged();
			});
		}
		if (!pProject)
		{
			Label(content, "Create a project to browse assets.");
			return root;
		}
		if (mCategory == 0)
		{
			auto& path = content.Add<UI::TextBox>(pProject->GetPath().string());
			path.OnCommitted = [this](const std::string& value) {
				std::error_code error;
				if (std::filesystem::is_directory(value, error))
					pProject->SetPath(value);
				else
					mStatus = "Directory is unavailable.";
				if (OnChanged)
					OnChanged();
			};
			Action(content, ".. / Parent directory", [this]() {
				pProject->SetPath(pProject->GetPath().parent_path());
				if (OnChanged)
					OnChanged();
			});
			std::error_code error;
			std::vector<std::filesystem::path> entries;
			for (std::filesystem::directory_iterator it(pProject->GetPath(), error), end; !error && it != end;
				 it.increment(error))
				entries.push_back(it->path());
			std::sort(entries.begin(), entries.end());
			if (error)
				Label(content, "Cannot read this directory: " + error.message()).SetWrap(true);
			for (const auto& entry : entries)
			{
				bool directory = std::filesystem::is_directory(entry, error);
				Action(content, (directory ? "[Folder] " : "Import / ") + entry.filename().string(),
					   [this, entry, directory]() {
						   if (directory)
							   pProject->SetPath(entry);
						   else
						   {
							   mPendingImport = entry;
							   mStatus = "Import queued: " + entry.filename().string();
						   }
						   if (OnChanged)
							   OnChanged();
					   });
			}
		}
		else
		{
			auto addMap = [this, &content](auto& map, int category) {
				std::vector<Assets::IAsset*> assets;
				for (auto& [id, asset] : map.mData)
					assets.push_back(&asset);
				std::sort(assets.begin(), assets.end(), [](auto* a, auto* b) {
					return a->GetName() < b->GetName();
				});
				if (assets.empty())
					Label(content, "No assets in this category.");

				if (category == 1 || category == 3) // Texture and material thumbnail buttons.
				{
					// Unreal-style grid of image buttons (thumbnail icon with asset name label underneath).
					UI::HorizontalBox* currentRow = nullptr;
					int count = 0;
					const int columns = 4;
					for (auto* asset : assets)
					{
						if (count % columns == 0)
						{
							currentRow = &content.Add<UI::HorizontalBox>();
							currentRow->SetSpacing(8);
						}
						auto* texture = category == 1 ? static_cast<Assets::Texture*>(asset)
							: mMeshPreviewCache.GetMaterialPreview(static_cast<Assets::Material*>(asset));
						const auto id = asset->GetUUID();
						UI::Brush brush;
						if (texture && texture->GetState() == Assets::IAsset::State::Created && texture->GetTextureView())
							brush = {texture};
						else
							brush = {nullptr, {.12f, .15f, .19f, 1}};

						const auto title = AssetDocument::GetTitle(asset->GetType(), asset->GetName());
						std::string subtext;
						if (category == 1 && texture && texture->GetState() == Assets::IAsset::State::Created)
							subtext = std::to_string(texture->GetWidth()) + " x " + std::to_string(texture->GetHeight());

						auto& item = currentRow->Add<EditorAssetItem>(AssetCommands(*asset));
						AssetTile(item, title, brush, {72, 72}, [this, id, type = asset->GetType()]() {
							if (OnAssetOpened)
								OnAssetOpened(type, id);
						}, subtext);
						++count;
					}
				}
				else if (category == 2) // Meshes
				{
					// Unreal-style grid of thumbnail buttons for meshes with cached 3D preview
					UI::HorizontalBox* currentRow = nullptr;
					int count = 0;
					const int columns = 4;
					for (auto* asset : assets)
					{
						if (count % columns == 0)
						{
							currentRow = &content.Add<UI::HorizontalBox>();
							currentRow->SetSpacing(8);
						}
						auto* mesh = static_cast<Assets::Mesh*>(asset);
						const auto id = asset->GetUUID();
						const auto title = AssetDocument::GetTitle(asset->GetType(), asset->GetName());

						Assets::Texture* previewTex = mMeshPreviewCache.GetPreview(mesh);
						UI::Brush brush;
						if (previewTex && previewTex->GetState() == Assets::IAsset::State::Created && previewTex->GetTextureView())
							brush = {previewTex};
						else
							brush = {nullptr, {.14f, .17f, .22f, 1}};

						size_t vertices = 0;
						size_t indices = 0;
						for (const auto& part : mesh->mSubMeshes)
						{
							vertices += part.mVB ? part.mVB->GetDesc().Size / sizeof(Assets::Mesh::Vertex) : part.data.Vertices.size();
							indices += part.mIndicesCount ? part.mIndicesCount : part.data.indices.size();
						}
						std::string subtext = std::to_string(indices / 3) + " tris";

						auto& item = currentRow->Add<EditorAssetItem>(AssetCommands(*asset));
						auto& cell = item.Add<UI::VerticalBox>();
						cell.SetSpacing(4);

						AssetTile(cell, title, brush, {72, 72}, [this, id]() {
							if (OnAssetOpened)
								OnAssetOpened(Assets::AssetType::Mesh, id);
						}, subtext, [this, id](UI::UIContext& context, UI::Vec2 point, bool dropped) {
							if (OnMeshDragged) OnMeshDragged(id, context, point, dropped);
						}, [this]() { if (OnMeshDragCancelled) OnMeshDragCancelled(); });

						if (mesh->GetState() == Assets::IAsset::State::Created)
						{
							auto& addBtn = Action(cell, "Add to scene / " + asset->GetName(), [this, id]() {
								AddToScene(id);
							});
							addBtn.SetPadding({4, 3, 4, 3});
						}

						++count;
					}
				}
				else // Scenes
				{
					for (auto* asset : assets)
					{
						const auto title = AssetDocument::GetTitle(asset->GetType(), asset->GetName());
						auto& item = content.Add<EditorAssetItem>(AssetCommands(*asset));
						auto& card = Section(item, title);
						Label(card, "State / " + std::to_string(static_cast<int>(asset->GetState())), 12);
					}
				}
			};
			auto& library = Assets::AssetLibrary::Get();
			if (mCategory == 1)
				addMap(library.mImportedTextures, 1);
			else if (mCategory == 2)
				addMap(library.mImportedMeshes, 2);
			else if (mCategory == 3)
				addMap(library.mImportedMaterials, 3);
			else
				addMap(library.mImportedScenes, 4);
		}
		return root;
	}
} // namespace Nuclear::Editor
