#pragma once
#include <Common.h>
#include <filesystem>
#include <Systems/RenderSystem.h>
#include <Rendering/RenderPasses/GeometryPass.h>
namespace Nuclear::Editor
{
	/// Project identity stored with the embedded startup scene.
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
		Assets::Scene* AddNewScene(); ///< Activates an empty working map with a fresh session ID. The returned storage lasts for the process.
		/// Creates shared editor/game shaders and the library-owned primitive material.
		void InitializeMaterials();
		/// Imports a model synchronously with its textures and records the source for saving.
		Assets::Mesh* ImportMesh(const std::filesystem::path& file);
		/// Resolves a built-in or source model. Share imports only across references in the same document read.
		Assets::IAsset* ResolveResource(const std::string& reference, bool material, const std::filesystem::path& directory,
			std::map<std::filesystem::path, Assets::Mesh*>& imports);
		/// Encodes a borrowed resource relative to the document directory, or throws if its source is unknown.
		std::string ResourceReference(Assets::IAsset* asset, const std::filesystem::path& directory) const;
		/// Creates the scene renderer after cameras/lights are installed. The caller owns the pass and renderer.
		std::unique_ptr<Systems::RenderSystem> BuildRenderer(Rendering::GeometryPass& geometry);
		/// Completes the library material for a freshly imported mesh on the graphics thread.
		void PrepareImportedMaterial(Assets::Mesh& mesh);
	  private:
		Assets::Shader* pShader = nullptr; ///< Borrowed shader for primitive meshes.
		Assets::Shader* pAnimatedShader = nullptr; ///< Forward PBR variants for models containing skeletal clips.
		Assets::Shader* pImportedShader = nullptr; ///< Borrowed shader for imported materials.
		std::unordered_map<Core::UUID, std::filesystem::path> mSources; ///< Mesh and material import origins.
		Assets::Material* mDefaultMaterial = nullptr; ///< Borrowed for the editor session.
		ProjectInfo mInfo;							  ///< Owned identity.
		std::filesystem::path mPath;				  ///< Current asset directory.
	};
} // namespace Nuclear::Editor
