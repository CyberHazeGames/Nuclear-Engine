#include <Core/Engine.h>
#include <Utilities\Timer.h>
#include <Platform\Window.h>
#include <Platform\Input.h>

#include <Utilities\Logger.h>

#include <Graphics/GraphicsModule.h>
#include <Assets\DefaultMeshes.h>

#include "..\Graphics\ImGUI\imgui_impl_sdl.h"
#include "..\Graphics\ImGUI\imgui_impl.h"

#include <SDL\include\SDL.h>

#include <filesystem>

//Engine Modules
#include <Audio\AudioModule.h>
#include <Graphics\GraphicsModule.h>
#include <PhysX\PhysXModule.h>
#include <Scripting/ScriptingModule.h>
#include <Rendering\RenderingModule.h>
#include <Threading/ThreadingModule.h>
#include <Threading/DelegateTask.h>

#include <Assets/AssetManager.h>
#include <Assets/AssetLibrary.h>

//Dependencies Linking
#pragma comment(lib,"assimp-vc143-mt.lib")

#pragma comment(lib,"freetype.lib")
#pragma comment(lib,"msdf-atlas-gen.lib")
#pragma comment(lib,"msdfgen.lib")

//Diligent Linking
#pragma comment(lib,"Diligent-Common.lib")

#ifdef _DEBUG
#pragma comment(lib,"GraphicsEngineD3D11_64d.lib")
#pragma comment(lib,"GraphicsEngineD3D12_64d.lib")
#pragma comment(lib,"GraphicsEngineOpenGL_64d.lib")
#pragma comment(lib,"GraphicsEngineVK_64d.lib")
#else
#pragma comment(lib,"GraphicsEngineD3D11_64r.lib")
#pragma comment(lib,"GraphicsEngineD3D12_64r.lib")
#pragma comment(lib,"GraphicsEngineOpenGL_64r.lib")
#pragma comment(lib,"GraphicsEngineVK_64r.lib")
#endif

#pragma comment(lib,"Diligent-GraphicsAccessories.lib")
#pragma comment(lib,"Diligent-GraphicsTools.lib")
#pragma comment(lib,"Diligent-BasicPlatform.lib")
#pragma comment(lib,"Diligent-Win32Platform.lib")


#pragma comment(lib,"SDL2.lib")
#pragma comment(lib,"SDL2Main.lib")


namespace Nuclear 
{
	namespace Core
	{

		static std::string MajorVersion = "0";
		static std::string MinorVersion = "001";

		void PrintIntroLog();

		void ResizeCallback(int Width, int Height)
		{
			if (Width == 0 && Height == 0)
			{
				return;
			}
			Engine::Get().GetClient()->OnWindowResize(Width, Height);
		}
		bool Engine::Start(const EngineStartupDesc& desc)
		{
			PrintIntroLog();
			Assets::AssetLibrary::Get().Initialize(desc.mAssetsLibraryPath);
			Core::Path::mReservedPaths["@Assets@"] = Assets::AssetLibrary::Get().GetPath();
			Core::Path::mReservedPaths["@NuclearAssets@"] = Assets::AssetLibrary::Get().GetPath() + "NuclearEngine";
			Core::Path::mReservedPaths["@CurrentPath@"] = std::filesystem::current_path().string();

			//Initialize SDL
			if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
			{
				NUCLEAR_FATAL("[Engine] SDL could not initialize! SDL_Error: {0}", SDL_GetError());
				return false;
			}

			if (!MainWindow.Create(desc.mEngineWindowDesc))
			{
				NUCLEAR_FATAL("[Engine] Failed To Create Window...");
			}

			Platform::Input::Get().SetMouseInputMode(Platform::Input::MouseInputMode::Normal);

			if (desc.AutoInitGraphicsModule)
			{
				Graphics::GraphicsModuleDesc GraphicsModuleDesc;
				GraphicsModuleDesc.pWindowHandle = GetMainWindow()->GetSDLWindowPtr();
				if (!Graphics::GraphicsModule::Get().Initialize(GraphicsModuleDesc))
				{
					NUCLEAR_FATAL("[Engine] Failed to initalize GraphicsModule...");
					return false;
				}
			}

			if (desc.AutoInitAudioModule)
			{
				Audio::AudioModuleDesc desc;
				desc.mRequestedBackend = Audio::AudioModuleDesc::AudioBackendType::XAudio2;

				if (!Audio::AudioModule::Get().Initialize(desc))
				{
					NUCLEAR_FATAL("[Engine] Failed to initalize AudioModule...");
				}
			}
			if (desc.AutoInitScriptingModule)
			{
				namespace fs = std::filesystem;

				Scripting::ScriptingModuleDesc scdesc;
				fs::path monopath = std::filesystem::current_path().string() + "/mono";

				if (!fs::exists(monopath))
				{
					monopath = std::filesystem::current_path().string() + "/../mono";
					if (!fs::exists(monopath))
					{
						NUCLEAR_FATAL("[Engine] Failed to find mono runtime assemblies directory!");
					}
				}
				scdesc.mMonoRuntimeDir = monopath.string();
				scdesc.mScriptingCoreAssemblyDir = std::filesystem::current_path().string();
				scdesc.mClientAssemblyPath = std::filesystem::current_path().string() + "/" + desc.mScriptingClientDllName;
				scdesc.mClientNamespace = desc.mScriptingAssemblyNamespace;
				if (!Scripting::ScriptingModule::Get().Initialize(scdesc))
				{
					NUCLEAR_FATAL("[Engine] Failed to initalize ScriptingModule...");
				}
			}

			if (desc.AutoInitPhysXModule)
			{
				PhysX::PhysXModuleDesc pxdesc;

				if (!PhysX::PhysXModule::Get().Initialize(pxdesc))
				{
					NUCLEAR_FATAL("[Engine] Failed to initalize PhysXModule...");
				}
			}

			if (desc.AutoInitRenderingModule)
			{
				Rendering::RenderingModuleDesc redesc;
				redesc.RTWidth = desc.mEngineWindowDesc.WindowWidth;
				redesc.RTHeight = desc.mEngineWindowDesc.WindowHeight;;
									
				if (!Rendering::RenderingModule::Get().Initialize(redesc))
				{
					NUCLEAR_FATAL("[Engine] Failed to initalize RenderingModule...");
				}			
			}

			if (desc.AutoInitThreadingModule)
			{
				if (!Threading::ThreadingModule::Get().Initialize())
				{
					NUCLEAR_FATAL("[Module] Failed to initalize ThreadingModule...");
				}
			}

			gisDebug = desc.Debug;

			Assets::AssetManager::Get().Initialize();

			NUCLEAR_INFO("[Engine] Nuclear Engine has been initialized successfully!");
			return true;
		}

		inline Engine& Engine::Get()
		{
			static Engine engine;

			return engine;
		}

		void Engine::Shutdown()
		{
			NUCLEAR_INFO("[Engine] Shutting Down Engine.");
			Assets::AssetLibrary::Get().Clear();
			pClient = nullptr;
			Threading::ThreadingModule::Get().Shutdown();
			Rendering::RenderingModule::Get().Shutdown();
			Audio::AudioModule::Get().Shutdown();
			PhysX::PhysXModule::Get().Shutdown();
			Graphics::GraphicsModule::Get().Shutdown();
			MainWindow.Destroy();
			//Graphics::ImGui_Renderer::Shutdown();
			SDL_Quit();
			NUCLEAR_INFO("------------------- -Nuclear Engine Has Shutdown- -----------------------");
		}

		void Engine::BeginFrame()
		{
			ImGui_Impl_NewFrame();
			ImGui_ImplSDL2_NewFrame();
			ImGui::NewFrame();
		}

		void Engine::EndFrame()
		{
			ImGui::Render();
			ImGui_Impl_RenderDrawData(ImGui::GetDrawData());
			Graphics::GraphicsModule::Get().GetSwapChain()->Present(SwapChainSyncInterval);
		}
		Platform::Window* Engine::GetMainWindow()
		{
			return &MainWindow;
		}

		void Engine::LoadClient(Client* client)
		{
			pClient = client;

			if (pClient != nullptr)
			{
				NUCLEAR_INFO("[Engine] Loading Client: '{0}' - Ver: '{1}' - Dev: '{2}'",
					pClient->GetClientInfo().mName,
					pClient->GetClientInfo().mVersion,
					pClient->GetClientInfo().mDeveloper);

				SetState(Engine::State::Initializing);
				pClient->Initialize();
				SetState(Engine::State::Loading);
				pClient->Load();

				Engine::MainLoop();
			}
		}

		void Engine::EndClient()
		{
			if (pClient != nullptr)
			{
				SetState(Engine::State::ExitingRendering);
				pClient->ExitRendering();
				SetState(Engine::State::Shuttingdown);
				pClient->Shutdown();
			}
			pClient = nullptr;
		}

		bool Engine::ShouldClose()
		{
			return mShouldClose;
		}

		bool Engine::isDebug()
		{
			return gisDebug;
		}

		Client* Engine::GetClient()
		{
			return pClient;
		}

		Engine::Engine()
			: SwapChainSyncInterval(0)
		{

		}

		void Engine::MainLoop()
		{
			SetState(Engine::State::Rendering);

			Utilities::Timer timer;

			mShouldClose = false;

			//Event handler
			SDL_Event e;
			//Main Client Loop
			while (!mShouldClose && pClient != nullptr)
			{
				while (SDL_PollEvent(&e) != 0)
				{
					switch (e.type)
					{
					case SDL_QUIT:
						mShouldClose = true;
						break;
					case SDL_WINDOWEVENT:
						if (e.window.event == SDL_WINDOWEVENT_RESIZED)
						{
							ResizeCallback(e.window.data1, e.window.data2);
						}
						break;
					case SDL_MOUSEMOTION:
						int w, h;
						MainWindow.GetSize(w,h);
						static int xpos = w / 2; 
						static int ypos = h / 2;
						xpos += e.motion.xrel;
						ypos += e.motion.yrel;
						pClient->OnMouseMovement(xpos, ypos);

						break;

					};
					ImGui_ImplSDL2_ProcessEvent(&e);
				}

				//Process MainThread tasks
				Threading::ThreadingModule::Get().ExecuteMainThreadTasks(1);
					
				//Render
				Platform::Input::Get().Update();

				// per-frame time logic (ensure speed is constant through all platforms)
				float currentFrame = static_cast<float>(timer.GetElapsedTimeInSeconds());
				pClient->DeltaTime = currentFrame - pClient->LastFrame;
				pClient->LastFrame = currentFrame;
				pClient->ClockTime = static_cast<float>(timer.GetElapsedTimeInSeconds());

				BeginFrame();

				pClient->Update(pClient->DeltaTime);
				pClient->Render(pClient->DeltaTime);

				EndFrame();

			}
		}

		void Engine::SetState(const State& state)
		{
			Engine_State = state;
			std::string name;
			switch (Engine_State)
			{
			case Engine::State::Initializing:
				name = "Initializing";
				break;
			case Engine::State::Loading:
				name = "Loading";
				break;
			case Engine::State::Rendering:
				name = "Rendering";
				break;
			case Engine::State::ExitingRendering:
				name = "Exiting Rendering";
				break;
			case Engine::State::Shuttingdown:
				name = "Shutting down";
				break;
			default:
				name = "Unknown";
				break;
			}

			NUCLEAR_INFO("[Engine] Client state changed to '{0}'", name);
		}

		void PrintIntroLog()
		{
			NUCLEAR_INFO("-------------------------- -Nuclear Engine- --------------------------");
			NUCLEAR_INFO("------------------------- Zone Organization --------------------------");
			NUCLEAR_INFO("--------------------------------v7medz--------------------------------");
			NUCLEAR_INFO("[Engine] Starting Engine...");
			NUCLEAR_INFO("[Engine] Engine Build: '{0}'.'{1}'  On: '{2}' At: '{3}'", MajorVersion, MinorVersion, __DATE__, __TIME__);

			//			NUCLEAR_INFO("[Engine] Built For: ");

			//#ifdef 	NUCLEAR_PLATFORM_WINDOWS_PC_32BIT
			//			NUCLEAR_INFO("Windows-PC 32 Bit");
			//#endif
			//
			//#ifdef 	NUCLEAR_PLATFORM_WINDOWS_PC_64BIT
			//			NUCLEAR_INFO("Windows-PC 64 bit");
			//#endif
			//
			//#ifdef 	_DEBUG
			//			NUCLEAR_INFO("  [DEBUG Build]\n");
			//#endif
			//
			//#ifdef 	NDEBUG
			//			NUCLEAR_INFO("  [RELEASE Build]\n");
			//#endif

#ifdef _MSC_VER
#pragma warning(disable : 4067)
#endif
		}
	}
}