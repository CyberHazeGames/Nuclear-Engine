#pragma once
#include <Common.h>
#include <Project.h>
#include <functional>
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
		std::function<void(entt::entity)> OnEntityAdded; ///< Selects a newly instantiated library mesh.
		std::function<void()> OnChanged; ///< Requests rebuilding browser content after navigation/import.
		std::function<void(Assets::AssetType, Core::UUID)> OnAssetOpened;
		///< Requests a details document using a stable library identity; does not transfer asset ownership.
	  private:
		Project* pProject = nullptr;		  ///< Borrowed for the editor session.
		std::filesystem::path mPendingImport; ///< Empty unless a user submitted an import.
		bool mOpenImportDialog = false; ///< Defers native file selection until Update.
		std::string mStatus =
			"Use Import... to select an asset for this project session."; ///< Browser feedback.
		int mCategory = 0; ///< Files, textures, meshes, materials or scenes.
	};
} // namespace Nuclear::Editor
