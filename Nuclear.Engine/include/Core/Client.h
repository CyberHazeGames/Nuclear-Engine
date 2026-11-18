#pragma once
#include <NE_Common.h>
#include <string>

namespace Nuclear::Systems { class PhysXSystem; }
namespace Nuclear::Platform { struct Event; }

namespace Nuclear
{
	namespace Assets {
		class AssetManager;
	}
	namespace Core
	{
		class Scene;
		
		enum class ClientType {
			Game,
			Editor,
			Test,
			Unknown
		};

		struct NEAPI ClientInfo
		{
			std::string mName;
			std::string mVersion;
			std::string mDeveloper;

			ClientType mType = ClientType::Game;
		};

		class NEAPI Client
		{
		public:
			Client(const ClientInfo& yourinfo = ClientInfo());
			virtual ~Client() = default;

			ClientInfo GetClientInfo();

			// Arranged function.
			//TODO: Old to be replaces...
			virtual void Initialize();	 // At Startup
			virtual void Load();          // Loading Assets
			virtual void Update(float Deltatime);        // Render The Game
			virtual void Render(float Deltatime);        // Render The Game
			/// Borrows the active simulation for gameplay queries while this client owns a world.
			virtual Systems::PhysXSystem* GetPhysicsWorld() { return nullptr; }
			/// Returns the name of the currently loaded authored level.
			virtual std::string GetLevelName() const { return {}; }
			/// Returns the persistent identity used to separate saved progress between games.
			virtual std::string GetGameName() const { return {}; }
			/// Pauses simulation while the owning script continues receiving UI updates.
			virtual bool SetGamePaused(Uint32, bool) { return false; }
			/// Reserves a prefab instance at a world position, returning the null entity ID on failure.
			virtual Uint32 SpawnPrefab(const std::string&, float, float, float) { return UINT32_MAX; }
			/// Queues a named level transition after this frame, returning false for an unknown level.
			virtual bool RequestLevel(const std::string& name) { return false; }
			virtual void ExitRendering(); // Exit Rendering
			virtual void Shutdown();      // At Shutdown

			/// Receives horizontal/vertical wheel steps, positive right/up respectively.
			virtual void OnMouseScroll(double xpos, double ypos);
			/// Receives accumulated relative mouse coordinates, initially at the window center.
			/// These virtual coordinates remain continuous when the cursor is locked/unlocked.
			virtual void OnMouseMovement(int xpos, int ypos);
			/// Receives nonzero client dimensions in pixels; minimized sizes are skipped.
			virtual void OnWindowResize(int width, int height);
			/// Receives events belonging to client-owned secondary native windows on the engine thread.
			/// The origin window is borrowed and must stay alive until its queued events are handled.
			/// Main-window and process Quit events follow the normal engine routing instead.
			virtual void OnAuxiliaryWindowEvent(const Platform::Event& event);

			// Helper Functions
			virtual void LoadFinishUp();

			static Core::Scene& GetScene();
			static Assets::AssetManager& GetAssetManager();

			float FPS, FrameTime, ClockTime;

			float DeltaTime = 0.0f;	// time between current frame and last frame
			float LastFrame = 0.0f;
		protected:
			ClientInfo minfo;
		};
	}
}
