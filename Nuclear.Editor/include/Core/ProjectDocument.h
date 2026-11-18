#pragma once
#include <Project.h>
#include <EditorEntityState.h>

namespace Nuclear::Editor
{
	/// An authored map and the project settings used when it is opened through a main project manifest.
	class ProjectDocument
	{
	public:
		/// Copies current authored entities, excluding the editor camera. Runtime handles are never saved.
		static ProjectDocument Capture(Project& project, entt::entity preview, entt::entity gameCamera);
		/// Parses and validates the whole file without replacing the current scene or loading assets.
		static ProjectDocument Read(const std::filesystem::path& file);
		/// Tests the supported map extension using Windows case-insensitive filename conventions.
		static bool IsMap(const std::filesystem::path& file);
		/// Tests whether the filename denotes a main project.
		static bool IsProject(const std::filesystem::path& file);
		/// Writes project settings and relative map references. The startup map must already exist.
		void SaveManifest(const std::filesystem::path& file) const;
		/// Copies legacy linked scenes to fresh map assets in the chosen directory and updates their links.
		void ConvertLegacyLevels(const std::filesystem::path& directory);
		/// Absolute startup map from a manifest. Empty for a legacy embedded startup scene.
		std::filesystem::path StartupMap;
		/// Prefabs local to the startup map when this document was read from a project manifest.
		std::map<std::string, std::filesystem::path> StartupPrefabs;
		/// Writes the scene as a map or prefab beside the destination and atomically replaces it.
		void Save(const std::filesystem::path& file, const Project& project) const;
		/// Imports all referenced resources before switching the live scene. Throws on a missing dependency.
		void Resolve(Project& project);
		/// Adds authored entities to a prepared scene and returns the primary camera, or null if unassigned.
		entt::entity Instantiate() const;
		std::string ProjectName, SceneName; ///< Owned project and embedded startup scene names.
		Math::Vector4 ClearColor{0, 0, 0, 1}; ///< Linear scene clear RGBA.
		std::filesystem::path Assembly; ///< Optional custom compiled gameplay DLL, otherwise the bundled behaviors.
		/// Named levels owned by the project. Paths are absolute in memory and relative in the file.
		std::map<std::string, std::filesystem::path> Levels;
        /// Single-entity prefab assets available to gameplay and the editor's placement command.
        std::map<std::string, std::filesystem::path> Prefabs;
        /// Captures one selected entity as a reusable asset with a fresh identity on placement.
        static ProjectDocument CapturePrefab(entt::entity entity);
        /// Returns the resolved single entity template or throws for a malformed prefab.
        EditorEntityState Template() const;
        bool NeedsPhysics() const; ///< True if any authored entity requests simulation.
		bool NeedsAudio() const; ///< True if a listener or configured source needs a backend.
		bool NeedsScripts() const; ///< True if any entity has a class assignment.
	private:
		friend class GameExporter; ///< Relocates source references in a private copy during export.
		/// Writes captured data with a live project, or imported reference strings after packaging.
		void Write(const std::filesystem::path& file, const Project* project) const;
		std::vector<EditorEntityState> mEntities; ///< Authored entities in file order.
		std::vector<std::pair<std::string, std::string>> mReferences; ///< Unresolved mesh/material references on read.
		std::filesystem::path mDirectory; ///< Absolute source directory for relative dependencies.
		int mCamera = -1; ///< File-local index of game camera, or -1 for an unassigned scene.
	};
}
