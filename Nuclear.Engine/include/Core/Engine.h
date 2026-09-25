#pragma once
#include <Core\Client.h>
#include <Platform\Window.h>
#include <Diligent/Graphics/GraphicsEngine/interface/GraphicsTypes.h>
#include <filesystem>
#include <memory>

namespace Nuclear::Core { class ModuleManager; }

namespace Nuclear
{
	namespace Core
	{
		struct EngineStartupDesc
		{
			Platform::WindowDesc mEngineWindowDesc;
			Uint32 Samples = 2;

			bool DebugRenderAPI = false;
			bool VSync = true;
			bool AutoSelectRenderer = true;

			bool AutoInitGraphicsModule = true;
			bool AutoInitAudioModule = true;
			bool AutoInitPhysXModule = true;
			bool AutoInitScriptingModule = true;
			bool AutoInitRenderingModule = true;
			bool AutoInitThreadingModule = true;
			bool AutoInitFallbacksModule = true;
			std::string mAudioBackendName = "XAudio2";
			std::filesystem::path mAudioPluginDirectory;
			std::filesystem::path mModulePluginDirectory;

			std::string mScriptingClientDllName = "ClientScripts.dll";
			std::string mScriptingAssemblyNamespace = "ClientScripts";
			std::string mAssetsLibraryPath = "Assets/";
			//Enables some debugging options that slows down performance in runtime
			bool Debug = DEBUG_TRUE_BOOL;
			bool EnableLogging = DEBUG_TRUE_BOOL;
		};

		class NEAPI Engine
		{
		public:
			Engine(Engine const&) = delete;
			void operator=(Engine const&) = delete;

			static Engine& Get();
			~Engine();

			enum class State { Initializing, Loading, Rendering, ExitingRendering, Shuttingdown };

			bool Start(const EngineStartupDesc& desc);

			void Shutdown();
			ModuleManager& GetModuleManager();

			void BeginFrame();

			void EndFrame();

			Platform::Window* GetMainWindow();

			void LoadClient(Client* client);
			void EndClient();

			void SetState(const State& state);

			bool ShouldClose();

			bool isDebug();

			Client* GetClient();

			FORCE_INLINE void SetSwapChainSyncInterval(Uint32 val)
			{
				SwapChainSyncInterval = val;
			}

		private:
			Engine();
			Client* pClient;
			Platform::Window MainWindow;

			Uint32 SwapChainSyncInterval;

			Engine::State Engine_State;
			bool gisDebug = DEBUG_TRUE_BOOL;
			bool mShouldClose = false;
			bool mThreadingEnabled = false;
			std::unique_ptr<ModuleManager> pModules;
			void MainLoop();
		};
	}
}
