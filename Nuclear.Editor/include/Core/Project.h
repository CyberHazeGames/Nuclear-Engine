#pragma once
#include <Common.h>
#include <filesystem>
namespace Nuclear::Editor
{
	/// In-memory project identity. Disk project serialization is not implemented.
	struct ProjectInfo
	{
		std::string mProjectName; ///< Display name owned by the project.
	};
	/// Session project model; scene and material assets are borrowed from the engine library.
	class Project
	{
	  public:
		Project();								 ///< Starts browsing the current working directory.
		void Initalize(const ProjectInfo& info); ///< Copies project identity.
		ProjectInfo GetProjectInfo() const;		 ///< Returns a copy of project identity.
		Assets::Material* GetDefaultMaterial();	 ///< Borrowed primitive material, or null.
		/// Returns the mesh's created, shader-backed imported material, otherwise the borrowed primitive default.
		/// The mesh and returned material remain owned by the asset library; does not create resources.
		Assets::Material* GetMeshMaterial(Assets::Mesh& mesh);
		void SetDefaultMaterial(Assets::Material* material); ///< Borrows a material for scene creation.
		void SetPath(const std::filesystem::path& path);	 ///< Stores a normalized browsing path.
		const std::filesystem::path& GetPath() const;		 ///< Borrowed browsing path.
		Assets::Scene* AddNewScene(); ///< Creates and activates a library-owned empty scene.
	  private:
		Assets::Material* mDefaultMaterial = nullptr; ///< Borrowed for the editor session.
		ProjectInfo mInfo;							  ///< Owned identity.
		std::filesystem::path mPath;				  ///< Current asset directory.
	};
} // namespace Nuclear::Editor
