#pragma once
#include <ProjectDocument.h>
#include <EditorSkybox.h>
#include <Systems/PhysXSystem.h>
#include <Systems/ScriptingSystem.h>
#include <Systems/AudioSystem.h>

namespace Nuclear::Editor
{
	/// Runs the current authored level in the editor Play process or the standalone Windows player.
	class ProjectPlayer : public Core::Client
	{
	public:
		/// Starts a complete game process using a saved project and the engine content directory.
		static int RunGame(const std::filesystem::path& file, const std::filesystem::path& assets);
		explicit ProjectPlayer(ProjectDocument document); ///< Owns the parsed document until shutdown.
		void Load() override; ///< Resolves content, activates the game camera and constructs gameplay services.
		Systems::PhysXSystem* GetPhysicsWorld() override { return mPhysics.get(); } ///< Current level physics.
		std::string GetLevelName() const override { return mDocument.SceneName; } ///< Current authored level.
		std::string GetGameName() const override { return mDocument.ProjectName; } ///< Owning game identity.
		bool SetGamePaused(Uint32 owner, bool paused) override; ///< Routes paused updates exclusively to the menu owner.
		Uint32 SpawnPrefab(const std::string& name, float x, float y, float z) override; ///< Activates the instance after script iteration.
		bool RequestLevel(const std::string& name) override; ///< Defers a known level transition until the frame finishes.
		void Update(float seconds) override; ///< Advances scripts, physics and audio, then follows the camera entity.
		void Render(float seconds) override; ///< Presents the camera target and the runtime HUD.
		void OnWindowResize(int width, int height) override; ///< Resizes drawable targets in physical pixels.
		void Shutdown() override; ///< Releases scene resources while graphics and optional services are live.
	private:
		void ActivatePhysics(entt::entity only = entt::null); ///< Creates a box collider from authored scale and an optional dynamic body.
		void ActivateAudio(entt::entity only = entt::null); ///< Loads assigned clips and binds sources/listeners to the audio system.
		void ActivateScripts(entt::entity only = entt::null); ///< Resolves every assigned class before invoking any start callback.
        std::map<std::string, EditorEntityState> mTemplates; ///< Resolved templates for the current level.
        std::vector<std::pair<entt::entity, EditorEntityState>> mSpawns; ///< Reserved identities pending component activation.
        entt::entity mPauseOwner = entt::null; ///< Script receiving menu updates while simulation is paused.
        std::string mNextLevel; ///< First valid level requested during this frame.
		ProjectDocument mDocument; ///< Parsed authored data.
		Project mProject; ///< Scene resource resolver.
		entt::entity mCamera = entt::null; ///< Primary authored camera in the live game registry.
		Rendering::GeometryPass mGeometry; ///< Borrowed by the renderer until shutdown.
		std::unique_ptr<Systems::RenderSystem> mRenderer; ///< Render bindings for this run.
		std::unique_ptr<Systems::PhysXSystem> mPhysics; ///< Optional live simulation.
		std::unique_ptr<Systems::ScriptingSystem> mScripts; ///< Optional managed lifecycle dispatcher.
		std::unique_ptr<Systems::AudioSystem> mAudio; ///< Optional live audio bindings.
		std::unique_ptr<EditorSkybox> mSky; ///< Optional scene background.
		float mPhysicsTime = 0; ///< Unsimulated seconds carried between frames.
	};
}
