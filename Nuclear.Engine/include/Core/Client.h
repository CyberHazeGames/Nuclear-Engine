#pragma once
#include <NE_Common.h>
#include <string>

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
			virtual void ExitRendering(); // Exit Rendering
			virtual void Shutdown();      // At Shutdown

			/// Receives horizontal/vertical wheel steps, positive right/up respectively.
			virtual void OnMouseScroll(double xpos, double ypos);
			/// Receives accumulated relative mouse coordinates, initially at the window center.
			/// These virtual coordinates remain continuous when the cursor is locked/unlocked.
			virtual void OnMouseMovement(int xpos, int ypos);
			/// Receives nonzero client dimensions in pixels; minimized sizes are skipped.
			virtual void OnWindowResize(int width, int height);

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
