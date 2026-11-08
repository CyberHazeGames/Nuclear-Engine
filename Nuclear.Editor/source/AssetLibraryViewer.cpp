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
			auto addMap = [this, &content](auto& map, bool textures) {
				std::vector<Assets::IAsset*> assets;
				for (auto& [id, asset] : map.mData)
					assets.push_back(&asset);
				std::sort(assets.begin(), assets.end(), [](auto* a, auto* b) {
					return a->GetName() < b->GetName();
				});
				if (assets.empty())
					Label(content, "No assets in this category.");
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
					if (asset->GetType() == Assets::AssetType::Mesh &&
						asset->GetState() == Assets::IAsset::State::Created)
					{
						const auto id = asset->GetUUID();
						Action(card, "Add to scene / " + asset->GetName(), [this, id]() {
							auto* mesh = Assets::AssetLibrary::Get().mImportedMeshes.GetAsset(id);
							if (!mesh || mesh->GetState() != Assets::IAsset::State::Created)
								return;
							auto entity = Core::Scene::Get().CreateEntity(mesh->GetName());
							entity.AddComponent<Components::MeshComponent>(mesh,
																		   pProject->GetDefaultMaterial());
							if (OnEntityAdded)
								OnEntityAdded(entity.GetEntityID());
						});
					}
					if (textures)
					{
						auto* texture = static_cast<Assets::Texture*>(asset);
						if (texture->GetState() == Assets::IAsset::State::Created &&
							texture->GetTextureView())
						{
							auto& preview = card.Add<UI::Image>();
							preview.SetBrush({texture});
							preview.SetImageSize({96, 96});
							auto slot = card.GetSlot(preview.GetId());
							slot.Horizontal = UI::Alignment::Start;
							card.SetSlot(preview.GetId(), slot);
							Label(card,
								  std::to_string(texture->GetWidth()) + " x " +
									  std::to_string(texture->GetHeight()),
								  12);
						}
					}
				}
			};
			auto& library = Assets::AssetLibrary::Get();
			if (mCategory == 1)
				addMap(library.mImportedTextures, true);
			else if (mCategory == 2)
				addMap(library.mImportedMeshes, false);
			else if (mCategory == 3)
				addMap(library.mImportedMaterials, false);
			else
				addMap(library.mImportedScenes, false);
		}
		return root;
	}
} // namespace Nuclear::Editor
