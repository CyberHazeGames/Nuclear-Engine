#pragma once
#include <Common.h>
#include <Project.h>
#include <MeshPreviewCache.h>
#include <functional>
#include <EditorMenuPopup.h>
namespace Nuclear::Editor
{
	/// Retained filesystem/import and library browser. Imports run from Update, outside input dispatch.
	class AssetLibraryViewer
	{
	  public:
		void SetProject(Project* project);	 ///< Borrows the active session project.
		/// Builds the browser. Maps include files below the project root and externally linked maps.
		std::unique_ptr<UI::Widget> Build(const std::filesystem::path& rootDirectory,
			const std::vector<std::filesystem::path>& linkedMaps);
		/// Opens a requested picker owned by dialogOwner, then executes a queued session import.
		/// Call on the engine/window thread, outside retained input dispatch; owner is borrowed for this call.
		void Update(Platform::Window& dialogOwner);
		/// Releases previews and retired assets after all widgets, scene entities and history have been cleared.
		void Shutdown();
		std::function<void(entt::entity)> OnEntityAdded; ///< Selects a newly instantiated library mesh.
		/// Opens an authored map or main project selected from the Files or Maps view during Update.
		std::function<void(const std::filesystem::path&)> OnDocumentOpened;
		std::function<void()> OnChanged; ///< Requests rebuilding browser content after navigation/import.
		/// Finalizes a created mesh's imported material on the graphics thread before browser refresh.
		/// Borrows the library-owned mesh for this call; source import and GPU task draining have completed.
		std::function<void(Assets::Mesh&)> OnMeshImported;
		std::function<void(Assets::AssetType, Core::UUID)> OnAssetOpened;
		///< Requests a details document using a stable library identity; does not transfer asset ownership.
		/// Updates document titles or closes a deleted asset's document; true means deleted.
		std::function<void(Assets::AssetType, Core::UUID, bool)> OnAssetChanged;
		/// Reports a ready mesh tile's captured drag in the source context; true means release.
		std::function<void(Core::UUID, UI::UIContext&, UI::Vec2, bool)> OnMeshDragged;
		std::function<void()> OnMeshDragCancelled; ///< Clears placement after Escape, source removal or focus loss.
	  private:
		/// Visible card widgets and lowercase search text, owned by one built browser tree.
		using SearchEntries = std::vector<std::pair<UI::Widget*, std::string>>;
		void BuildNavigation(UI::PanelWidget& bar); ///< Builds navigation controls for the active source.
		void BuildSources(UI::PanelWidget& body); ///< Adds the library categories and the active directory branch.
		void BuildFiles(UI::PanelWidget& grid, SearchEntries& entries); ///< Builds actionable filesystem cards.
		/// Collects distinct saved map files from the content directory and the project manifest.
		void BuildMaps(UI::PanelWidget& grid, SearchEntries& entries, const std::filesystem::path& rootDirectory,
			const std::vector<std::filesystem::path>& linkedMaps);
		/// Opens the file during Update. The caption can disambiguate maps with the same filename.
		void AddMapCard(UI::PanelWidget& grid, SearchEntries& entries, const std::filesystem::path& file,
			const std::string& caption);
		void BuildLibrary(UI::PanelWidget& grid, SearchEntries& entries); ///< Builds cards for the active library type.
		/// Selects a directory or imported asset category and records the previous location.
		void Navigate(int category, const std::filesystem::path& directory);
		/// Restores a browser location by index and schedules a rebuild after dispatch.
		void Visit(size_t index);
		/// Project content root used for the folder tree and breadcrumb trail.
		std::filesystem::path mRoot;
		/// Session navigation history. Each location preserves the active source and filesystem folder.
		std::vector<std::pair<int, std::filesystem::path>> mHistory;
		size_t mLocation = 0; ///< Current index in the navigation history.
		std::string mSearch; ///< Case-insensitive query within the active source.
		std::string mFileType; ///< Active file type filter, empty to include every type.
		bool mShowFilters = false; ///< Displays the available file type choices.
		bool mShowSources = true; ///< User preference retained across dock rebuilds.
		bool mEditPath = false; ///< Replaces the breadcrumb trail with an editable directory.
		/// Builds UUID-based commands; Add to scene is available only for created meshes.
		EditorMenuPopup::Items AssetCommands(Assets::IAsset& asset);
		void AddToScene(const Core::UUID& id); ///< Re-resolves a mesh before creating a scene entity.
		void DeleteAsset(Assets::AssetType type, const Core::UUID& id); ///< Removes a session entry, retaining borrowed resources.
		void RenameAsset(const std::string& name); ///< Commits a nonblank session name without changing UUID or disk files.
		Assets::IAsset* ResolveAsset(Assets::AssetType type, const Core::UUID& id); ///< Borrowed session asset, or null.
		std::vector<std::shared_ptr<void>> mRetiredAssets; ///< Owns extracted map nodes so scene/history/preview pointers stay valid until Shutdown.
		bool mRenaming = false; ///< Shows the browser's rename form until Save or Cancel.
		Assets::AssetType mRenameType = Assets::AssetType::Unknown; ///< Type of the pending rename target.
		Core::UUID mRenameId; ///< Stable pending rename identity, re-resolved at commit.
		std::string mRenameText; ///< Uncommitted rename text preserved across browser rebuilds.
		Project* pProject = nullptr;		  ///< Borrowed for the editor session.
		std::filesystem::path mPendingImport; ///< Empty unless a user submitted an import.
		bool mOpenImportDialog = false; ///< Defers native file selection until Update.
		std::string mStatus; ///< Last import, navigation error or library action.
		int mCategory = 0; ///< Files, textures, meshes, materials or maps.
		MeshPreviewCache mMeshPreviewCache; ///< Owns GPU mesh and material-sphere thumbnail textures for the browser.
	};
} // namespace Nuclear::Editor
