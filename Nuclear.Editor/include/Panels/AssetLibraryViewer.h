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
		std::unique_ptr<UI::Widget> Build(); ///< Creates the owning browser tree.
		/// Opens a requested picker owned by dialogOwner, then executes a queued session import.
		/// Call on the engine/window thread, outside retained input dispatch; owner is borrowed for this call.
		void Update(Platform::Window& dialogOwner);
		/// Releases previews and retired assets after all widgets, scene entities and history have been cleared.
		void Shutdown();
		std::function<void(entt::entity)> OnEntityAdded; ///< Selects a newly instantiated library mesh.
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
		std::string mStatus =
			"Use Import... to select an asset for this project session."; ///< Browser feedback.
		int mCategory = 0; ///< Files, textures, meshes, materials or scenes.
		MeshPreviewCache mMeshPreviewCache; ///< Owns GPU mesh and material-sphere thumbnail textures for the browser.
	};
} // namespace Nuclear::Editor
