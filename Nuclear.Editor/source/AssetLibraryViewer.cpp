#include <AssetLibraryViewer.h>
#include <EditorWidgets.h>
#include <AssetDocument.h>
#include <Nuclear.Platform/FileDialog.h>
#include <algorithm>
#include <cctype>
#include <Threading/ThreadingModule.h>
#include <Fallbacks/FallbacksModule.h>
namespace Nuclear::Editor
{
	void AssetLibraryViewer::SetProject(Project* project)
	{
		pProject = project;
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
				desc.mImportMaterial = false;
				desc.mImportAnimations = false;
				desc.mCommonOptions.mAsyncImport = false;
				desc.mCommonOptions.mLoadOnly = true;
				result = manager.Import<Assets::Mesh>(path.string(), desc);
				// Synchronous mesh decoding still queues its GPU creation on the main thread.
				auto& threading = Threading::ThreadingModule::Get();
				while (!threading.GetMainTasks().empty())
					threading.ExecuteMainThreadTasks(1);
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
		auto& actions = root->Add<UI::HorizontalBox>();
		auto& import = Action(actions, "Import...", [this]() {
			mOpenImportDialog = true;
		});
		import.SetEnabled(pProject != nullptr);
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
		}
		auto& scroll = root->Add<UI::ScrollBox>();
		Fill(*root, scroll);
		auto& content = scroll.Add<UI::VerticalBox>();
		content.SetSpacing(6);
		Label(content, mStatus, 13).SetWrap(true);
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

				if (category == 1) // Textures
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
						auto* texture = static_cast<Assets::Texture*>(asset);
						const auto id = asset->GetUUID();
						UI::Brush brush;
						if (texture && texture->GetState() == Assets::IAsset::State::Created && texture->GetTextureView())
							brush = {texture};
						else
							brush = {nullptr, {.12f, .15f, .19f, 1}};

						const auto title = AssetDocument::GetTitle(asset->GetType(), asset->GetName());
						std::string subtext;
						if (texture && texture->GetState() == Assets::IAsset::State::Created)
							subtext = std::to_string(texture->GetWidth()) + " x " + std::to_string(texture->GetHeight());

						AssetTile(*currentRow, title, brush, {72, 72}, [this, id]() {
							if (OnAssetOpened)
								OnAssetOpened(Assets::AssetType::Texture, id);
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

						auto& cell = currentRow->Add<UI::VerticalBox>();
						cell.SetSpacing(4);

						AssetTile(cell, title, brush, {72, 72}, [this, id]() {
							if (OnAssetOpened)
								OnAssetOpened(Assets::AssetType::Mesh, id);
						}, subtext);

						if (mesh->GetState() == Assets::IAsset::State::Created)
						{
							auto& addBtn = Action(cell, "Add to scene / " + asset->GetName(), [this, id]() {
								auto* mesh = Assets::AssetLibrary::Get().mImportedMeshes.GetAsset(id);
								if (!mesh || mesh->GetState() != Assets::IAsset::State::Created)
									return;
								auto entity = Core::Scene::Get().CreateEntity(mesh->GetName());
								entity.AddComponent<Components::MeshComponent>(mesh, pProject->GetDefaultMaterial());
								if (OnEntityAdded)
									OnEntityAdded(entity.GetEntityID());
							});
							addBtn.SetPadding({4, 3, 4, 3});
						}

						++count;
					}
				}
				else // Materials, Scenes
				{
					for (auto* asset : assets)
					{
						const auto title = AssetDocument::GetTitle(asset->GetType(), asset->GetName());
						auto& card = Section(content, title);
						Label(card, "State / " + std::to_string(static_cast<int>(asset->GetState())), 12);
						if (asset->GetType() == Assets::AssetType::Texture || asset->GetType() == Assets::AssetType::Mesh ||
							asset->GetType() == Assets::AssetType::Material)
						{
							const auto id = asset->GetUUID();
							const auto type = asset->GetType();
							Action(card, "Open / " + title, [this, id, type]() {
								if (OnAssetOpened) OnAssetOpened(type, id);
							});
						}
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
