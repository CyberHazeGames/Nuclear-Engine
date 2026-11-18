#include <ProjectDocument.h>
#include <AssetLibraryViewer.h>
#include <EditorSettings.h>
#include <EditorWidgets.h>
#include <AssetDocument.h>
#include <EditorAssetItem.h>
#include <AssetFileIcon.h>
#include <AssetGridView.h>
#include <EditorToolbar.h>
#include <Nuclear.Platform/FileDialog.h>
#include <algorithm>
#include <cctype>
#include <Threading/ThreadingModule.h>
#include <Fallbacks/FallbacksModule.h>
namespace Nuclear::Editor
{
	namespace
	{
		constexpr const char* Sources[] = {"Files", "Textures", "Meshes", "Materials", "Maps"};
		constexpr UI::Color Gold{.93f, .65f, .30f, 1};
		constexpr UI::Color Blue{.31f, .61f, .90f, 1};
		constexpr UI::Color Green{.36f, .70f, .52f, 1};
		constexpr UI::Color Violet{.69f, .50f, .84f, 1};
		constexpr UI::Color Silver{.55f, .63f, .71f, 1};

		std::string Lower(std::string value)
		{
			for (char& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			return value;
		}

		std::string FolderName(const std::filesystem::path& path)
		{
			if (!path.filename().empty()) return path.filename().string();
			if (path == path.root_path()) return path.root_name().string();
			return path.parent_path().filename().string();
		}

		UI::Button& BrowserAction(UI::PanelWidget& parent, const std::string& text,
			std::function<void()> clicked, bool active = false)
		{
			auto& button = parent.Add<UI::Button>();
			button.SetSizeLimits({0, 22}, {10000, 24});
			button.SetPadding({7, 3, 7, 3});
			UI::ControlStyle style;
			style.Normal = active ? UI::Color{.105f, .245f, .40f, 1} : UI::Color{.085f, .099f, .119f, 1};
			style.Hovered = {.15f, .21f, .28f, 1};
			style.Pressed = {.09f, .22f, .36f, 1};
			style.Focus = EditorSettings::Get().GetAccentColor();
			button.SetStyle(style);
			Label(button, text, 12);
			button.OnClicked = std::move(clicked);
			return button;
		}

		std::vector<std::filesystem::directory_entry> Entries(const std::filesystem::path& dir,
			bool foldersOnly, std::error_code& error)
		{
			std::vector<std::filesystem::directory_entry> entries;
			for (std::filesystem::directory_iterator it(dir, error), end; !error && it != end; it.increment(error))
			{
				std::error_code status;
				if (!foldersOnly || it->is_directory(status)) entries.push_back(*it);
			}
			std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
				std::error_code firstError, secondError;
				const bool first = a.is_directory(firstError), second = b.is_directory(secondError);
				return first != second ? first : a.path().filename() < b.path().filename();
			});
			return entries;
		}
	}

	void AssetLibraryViewer::Navigate(int category, const std::filesystem::path& directory)
	{
		std::error_code error;
		if (!std::filesystem::is_directory(directory, error))
		{
			mStatus = "Directory is unavailable.";
			if (OnChanged) OnChanged();
			return;
		}
		auto location = std::make_pair(category, directory.lexically_normal());
		if (mHistory.empty() || mHistory[mLocation] != location)
		{
			if (!mHistory.empty()) mHistory.resize(mLocation + 1);
			mHistory.push_back(std::move(location));
			Visit(mHistory.size() - 1);
		}
		else Visit(mLocation);
	}

	void AssetLibraryViewer::Visit(size_t index)
	{
		if (index >= mHistory.size() || !pProject) return;
		mLocation = index;
		mCategory = mHistory[index].first;
		pProject->SetPath(mHistory[index].second);
		mEditPath = false;
		mStatus.clear();
		if (OnChanged) OnChanged();
	}

	Assets::IAsset* AssetLibraryViewer::ResolveAsset(Assets::AssetType type, const Core::UUID& id)
	{
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
		mRoot = project ? project->GetPath() : std::filesystem::path{};
		mHistory.clear();
		mLocation = 0;
		mCategory = 0;
		mSearch.clear();
		mFileType.clear();
		mShowFilters = false;
		mEditPath = false;
		mStatus.clear();
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
			if (OnDocumentOpened && (ProjectDocument::IsMap(path) || ProjectDocument::IsProject(path)))
			{
				OnDocumentOpened(path);
				mStatus = "Opened " + path.filename().string();
				if (OnChanged) OnChanged();
				return;
			}
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
				result = pProject->ImportMesh(path);
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

	void AssetLibraryViewer::BuildNavigation(UI::PanelWidget& bar)
	{
		const auto directory = pProject->GetPath();
		BrowserAction(bar, "Sources", [this]() {
			mShowSources = !mShowSources;
			if (OnChanged) OnChanged();
		}, mShowSources);
		BrowserAction(bar, "Import...", [this]() { mOpenImportDialog = true; });
		BrowserAction(bar, "<", [this]() { if (mLocation > 0) Visit(mLocation - 1); }).SetEnabled(mLocation > 0);
		BrowserAction(bar, ">", [this]() { Visit(mLocation + 1); }).SetEnabled(mLocation + 1 < mHistory.size());
		if (mEditPath)
		{
			auto& path = bar.Add<UI::TextBox>(directory.string());
			Fill(bar, path);
			path.OnCommitted = [this](const std::string& value) { Navigate(0, value); };
		}
		else
		{
			auto& crumbs = bar.Add<UI::HorizontalBox>();
			Fill(bar, crumbs);
			crumbs.SetSpacing(2);
			auto crumb = [&](const std::filesystem::path& folder) {
				auto& button = BrowserAction(crumbs, FolderName(folder), [this, folder]() { Navigate(0, folder); });
				button.SetSizeLimits({0, 22}, {110, 24});
			};
			if (mCategory == 0)
			{
				auto end = directory.filename().empty() && directory != directory.root_path() ? directory.parent_path() : directory;
				if (end != end.root_path() && end != mRoot && !end.parent_path().empty())
				{
					crumb(end.parent_path());
					Label(crumbs, "/", 12);
				}
				crumb(end);
			}
			else Label(crumbs, std::string("Library / ") + Sources[mCategory], 12);
		}
		BrowserAction(bar, mEditPath ? "Cancel" : "Path", [this]() {
			mEditPath = !mEditPath;
			if (OnChanged) OnChanged();
		});
	}

	void AssetLibraryViewer::BuildSources(UI::PanelWidget& body)
	{
		auto& rail = body.Add<UI::Border>();
		rail.SetSizeLimits({112, 0}, {112, 100000});
		rail.SetBrush({nullptr, {.055f, .064f, .079f, 1}});
		rail.SetPadding({4, 3, 2, 2});
		auto& scroll = rail.Add<UI::ScrollBox>();
		auto& column = scroll.Add<UI::VerticalBox>();
		column.SetSpacing(1);
		const auto directory = pProject->GetPath();
		BrowserAction(column, "Files", [this, directory]() { Navigate(0, directory); });
		auto current = directory.lexically_normal();
		if (current.filename().empty() && current != current.root_path()) current = current.parent_path();
		auto root = mRoot.lexically_normal();
		if (root.filename().empty() && root != root.root_path()) root = root.parent_path();
		auto relative = current.lexically_relative(root);
		if (relative.empty() || *relative.begin() == "..") root = current;
		auto branch = [&](const std::filesystem::path& path, size_t depth) {
			auto remainder = current.lexically_relative(path);
			const bool expanded = !remainder.empty() && *remainder.begin() != "..";
			auto& node = BrowserAction(column, (expanded ? "v  " : ">  ") + FolderName(path), [this, path]() { Navigate(0, path); },
				mCategory == 0 && current == path);
			node.SetPadding({float(4 + 7 * std::min<size_t>(depth, 6)), 3, 1, 3});
		};
		auto expand = [&](auto& self, const std::filesystem::path& folder, size_t depth) -> void {
			std::error_code error;
			for (const auto& entry : Entries(folder, true, error))
			{
				branch(entry.path(), depth);
				auto remainder = current.lexically_relative(entry.path());
				if (!remainder.empty() && *remainder.begin() != "..") self(self, entry.path(), depth + 1);
			}
		};
		branch(root, 0);
		expand(expand, root, 1);
		auto& library = Label(column, "Library", 11);
		library.SetPadding({4, 6, 0, 2});
		for (int i = 1; i < 5; ++i)
			BrowserAction(column, Sources[i], [this, i, directory]() { Navigate(i, directory); }, i == mCategory);
	}

	std::unique_ptr<UI::Widget> AssetLibraryViewer::Build(const std::filesystem::path& rootDirectory,
		const std::vector<std::filesystem::path>& linkedMaps)
	{
		auto root = std::make_unique<UI::VerticalBox>();
		root->SetSpacing(3);
		if (!pProject)
		{
			Label(*root, "Create a project to browse assets.");
			return root;
		}

		if (mHistory.empty() || mHistory[mLocation].second != pProject->GetPath())
		{
			mRoot = pProject->GetPath();
			mHistory = {{mCategory, mRoot}};
			mLocation = 0;
		}
		auto& bar = root->Add<UI::HorizontalBox>();
		bar.SetSpacing(3);
		BuildNavigation(bar);
		auto& body = root->Add<UI::HorizontalBox>();
		Fill(*root, body);
		body.SetSpacing(4);
		if (mShowSources) BuildSources(body);
		auto& pane = body.Add<UI::VerticalBox>();
		Fill(body, pane);
		pane.SetSpacing(3);
		auto& searchBar = pane.Add<UI::HorizontalBox>();
		searchBar.SetSpacing(5);
		Label(searchBar, "Search", 12);
		auto& search = searchBar.Add<UI::TextBox>(mSearch);
		Fill(searchBar, search);
		if (mCategory == 0)
		{
			BrowserAction(searchBar, mFileType.empty() ? "Filter" : mFileType, [this]() {
				mShowFilters = !mShowFilters;
				if (OnChanged) OnChanged();
			}, !mFileType.empty() || mShowFilters);
			if (mShowFilters)
			{
				auto& choices = pane.Add<EditorToolbar>();
				for (const std::string type : {"All", "Folder", "Map", "Project", "Mesh", "Texture", "Material", "Audio", "Prefab", "Other"})
					BrowserAction(choices, type, [this, type]() {
						mFileType = type == "All" ? "" : type;
						mShowFilters = false;
						if (OnChanged) OnChanged();
					}, mFileType == type || (mFileType.empty() && type == "All"));
			}
		}
		auto& scroll = pane.Add<AssetGridView>();
		Fill(pane, scroll);
		auto& content = scroll.Add<UI::VerticalBox>();
		content.SetSpacing(4);
		if (mRenaming)
		{
			auto& form = Section(content, "Rename asset");
			auto& name = form.Add<UI::TextBox>(mRenameText);
			name.OnChanged = [this](const std::string& value) { mRenameText = value; };
			name.OnCommitted = [this](const std::string& value) { RenameAsset(value); };
			auto& actions = form.Add<UI::HorizontalBox>();
			BrowserAction(actions, "Save name", [this]() { RenameAsset(mRenameText); });
			BrowserAction(actions, "Cancel rename", [this]() { mRenaming = false; if (OnChanged) OnChanged(); });
		}
		auto& grid = content.Add<EditorToolbar>();
		std::vector<std::pair<UI::Widget*, std::string>> entries;
		if (mCategory == 0) BuildFiles(grid, entries);
		else if (mCategory == 4) BuildMaps(grid, entries, rootDirectory, linkedMaps);
		else BuildLibrary(grid, entries);

		auto& empty = Label(content, "No items", 12);
		auto& footer = root->Add<UI::HorizontalBox>();
		footer.SetSpacing(10);
		auto& count = Label(footer, "", 11);
		if (!mStatus.empty()) Label(footer, mStatus, 11);
		search.OnChanged = [this, entries = std::move(entries), &count, &empty, &scroll](const std::string& text) {
			mSearch = text;
			const auto query = Lower(text);
			size_t visible = 0;
			for (const auto& [widget, key] : entries)
			{
				const bool match = key.find(query) != std::string::npos;
				widget->SetVisibility(match ? UI::Visibility::Visible : UI::Visibility::Collapsed);
				if (match) ++visible;
			}
			count.SetText(std::to_string(visible) + (visible == 1 ? " item" : " items"));
			empty.SetText(text.empty() ? "No items" : "No matching items");
			empty.SetVisibility(visible ? UI::Visibility::Collapsed : UI::Visibility::Visible);
			scroll.SetOffset(0);
		};
		search.OnChanged(mSearch);
		return root;
	}

	void AssetLibraryViewer::BuildFiles(UI::PanelWidget& grid, SearchEntries& entries)
	{
		std::error_code error;
		for (const auto& entry : Entries(pProject->GetPath(), false, error))
		{
			const auto path = entry.path();
			std::error_code status;
			const bool folder = entry.is_directory(status);
			if (!folder && ProjectDocument::IsMap(path))
			{
				if (mFileType.empty() || mFileType == "Map") AddMapCard(grid, entries, path, "Map");
				continue;
			}
			const bool project = ProjectDocument::IsProject(path);
			auto extension = path.extension().string();
			if (!extension.empty()) extension.erase(0, 1);
			auto ext = Lower(extension);
			std::string type = "Other";
			if (folder) type = "Folder";
			else if (project) type = "Project";
			else if (ext == "nuclearprefab") type = "Prefab";
			else if (ext == "nuclearmaterial" || ext == "nematerial") type = "Material";
			else if (ext == "obj" || ext == "fbx" || ext == "dae" || ext == "gltf" || ext == "glb") type = "Mesh";
			else if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" || ext == "tga" || ext == "hdr") type = "Texture";
			else if (ext == "wav" || ext == "ogg" || ext == "flac") type = "Audio";
			if (!mFileType.empty() && type != mFileType) continue;
			auto format = extension.substr(0, 4);
			for (auto& c : format) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
			const auto caption = type == "Other" ? (format.empty() ? "File" : format + " file") : type;
			const auto accent = folder ? Gold : project ? Violet : type == "Prefab" ? Blue : Silver;
			auto icon = std::make_unique<AssetFileIcon>(folder, project ? "PROJ" : type == "Prefab" ? "PFB" : format, accent);
			const bool authored = project || type == "Prefab" || type == "Material";
			const auto title = authored ? path.stem().string() : path.filename().string();
			auto& card = AssetTile(grid, title, {}, {88, 48}, [this, path, folder]() {
				if (folder) Navigate(0, path);
				else mPendingImport = path;
			}, caption, {}, {}, std::move(icon), accent);
			entries.emplace_back(&card, Lower(path.filename().string() + " " + type));
		}
		if (error) mStatus = "Cannot read directory: " + error.message();
	}

	void AssetLibraryViewer::AddMapCard(UI::PanelWidget& grid, SearchEntries& entries,
		const std::filesystem::path& file, const std::string& caption)
	{
		auto icon = std::make_unique<AssetFileIcon>(false, "MAP", Green);
		auto& card = AssetTile(grid, file.stem().string(), {}, {88, 48}, [this, file]() {
			mPendingImport = file;
		}, caption, {}, {}, std::move(icon), Green);
		entries.emplace_back(&card, Lower(file.filename().string() + " Map " + caption));
	}

	void AssetLibraryViewer::BuildMaps(UI::PanelWidget& grid, SearchEntries& entries,
		const std::filesystem::path& rootDirectory, const std::vector<std::filesystem::path>& linkedMaps)
	{
		namespace fs = std::filesystem;
		std::map<std::string, fs::path> paths;
		auto collect = [&](const fs::path& file) {
			std::error_code error;
			if (!ProjectDocument::IsMap(file) || !fs::is_regular_file(file, error)) return;
			auto path = fs::canonical(file, error);
			if (!error) paths.emplace(Lower(path.generic_string()), path);
		};
		for (const auto& file : linkedMaps) collect(file);
		std::error_code error;
		for (fs::recursive_directory_iterator it(rootDirectory, fs::directory_options::skip_permission_denied, error), end;
			it != end && !error; it.increment(error))
			collect(it->path());
		if (error) mStatus = "Cannot read map directory: " + error.message();

		std::map<std::string, size_t> counts;
		for (const auto& [key, path] : paths) ++counts[Lower(path.filename().string())];
		for (const auto& [key, path] : paths)
		{
			auto caption = std::string("Map");
			if (counts[Lower(path.filename().string())] > 1)
			{
				auto folder = path.parent_path().lexically_relative(rootDirectory);
				caption = (folder.empty() ? path.parent_path() : folder).generic_string();
			}
			AddMapCard(grid, entries, path, caption);
		}
	}

	void AssetLibraryViewer::BuildLibrary(UI::PanelWidget& grid, SearchEntries& entries)
	{
		auto& library = Assets::AssetLibrary::Get();
		std::vector<Assets::IAsset*> assets;
		auto collect = [&](auto& map) { for (auto& [id, asset] : map.mData) assets.push_back(&asset); };
		switch (mCategory)
		{
		case 1: collect(library.mImportedTextures); break;
		case 2: collect(library.mImportedMeshes); break;
		case 3: collect(library.mImportedMaterials); break;
		}
		std::sort(assets.begin(), assets.end(), [](auto* a, auto* b) { return a->GetName() < b->GetName(); });
		for (auto* asset : assets)
		{
			const auto id = asset->GetUUID();
			const auto type = asset->GetType();
			auto& item = grid.Add<EditorAssetItem>(AssetCommands(*asset));
			item.SetSizeLimits({96, 0}, {96, 10000});
			entries.emplace_back(&item, Lower(asset->GetName() + " " + Sources[mCategory]));
			auto& cell = item.Add<UI::VerticalBox>();
			Assets::Texture* preview = nullptr;
			EditorAssetButton::DragCallback drag;
			std::string caption;
			auto accent = Blue;
			if (mCategory == 1)
			{
				preview = static_cast<Assets::Texture*>(asset);
				accent = Green;
				caption = "Texture";
				if (preview->GetState() == Assets::IAsset::State::Created)
					caption += "  " + std::to_string(preview->GetWidth()) + " x " + std::to_string(preview->GetHeight());
			}
			else if (mCategory == 2)
			{
				auto* mesh = static_cast<Assets::Mesh*>(asset);
				preview = mMeshPreviewCache.GetPreview(mesh);
				size_t indices = 0;
				for (const auto& part : mesh->mSubMeshes)
					indices += part.mIndicesCount ? part.mIndicesCount : part.data.indices.size();
				caption = "Mesh  " + std::to_string(indices / 3) + " tris";
				drag = [this, id](UI::UIContext& context, UI::Vec2 point, bool dropped) {
					if (OnMeshDragged) OnMeshDragged(id, context, point, dropped);
				};
			}
			else
			{
				preview = mMeshPreviewCache.GetMaterialPreview(static_cast<Assets::Material*>(asset));
				caption = "Material";
				accent = Violet;
			}

			UI::Brush brush{nullptr, {.07f, .08f, .10f, 1}};
			if (preview && preview->GetState() == Assets::IAsset::State::Created && preview->GetTextureView()) brush = {preview};
			AssetTile(cell, AssetDocument::GetTitle(type, asset->GetName()), brush, {88, 48}, [this, id, type]() {
				if (OnAssetOpened) OnAssetOpened(type, id);
			}, caption, std::move(drag), [this]() { if (OnMeshDragCancelled) OnMeshDragCancelled(); }, {}, accent);
			if (mCategory == 2 && asset->GetState() == Assets::IAsset::State::Created)
				BrowserAction(cell, "Place in scene", [this, id]() { AddToScene(id); });
		}
	}
} // namespace Nuclear::Editor
