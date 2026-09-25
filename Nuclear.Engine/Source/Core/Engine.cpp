#include <Core/Engine.h>
#include <Core/ModuleManager.h>
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
#include <CSharpScriptingBackend.h>
#include <Rendering\RenderingModule.h>
#include <Threading/ThreadingModule.h>
#include <Threading/DelegateTask.h>
#include <Fallbacks/FallbacksModule.h>

#include <Assets/AssetManager.h>
#include <Assets/AssetLibrary.h>

//Dependencies Linking
// Assimp's toolset-specific library name is selected by the project.

#pragma comment(lib,"freetype.lib")
#pragma comment(lib,"msdf-atlas-gen.lib")
#pragma comment(lib,"msdfgen-core.lib")
#pragma comment(lib,"msdfgen-ext.lib")

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
#pragma comment(lib,"Diligent-GraphicsEngineD3DBase.lib")
#pragma comment(lib,"Diligent-GraphicsEngineOpenGL-static.lib")
#pragma comment(lib,"Diligent-GraphicsEngineVk-static.lib")
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
			auto failStartup = [this]() { Shutdown(); return false; };
			Assets::AssetLibrary::Get().Initialize(desc.mAssetsLibraryPath);
			Core::Path::mReservedPaths["@Assets@"] = Assets::AssetLibrary::Get().GetPath();
			Core::Path::mReservedPaths["@NuclearAssets@"] = Assets::AssetLibrary::Get().GetPath() + "NuclearEngine";
			Core::Path::mReservedPaths["@CurrentPath@"] = std::filesystem::current_path().string();

			//Initialize SDL
			if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
			{
				NUCLEAR_FATAL("[Engine] SDL could not initialize! SDL_Error: {0}", SDL_GetError());
				return failStartup();
			}

			if (!MainWindow.Create(desc.mEngineWindowDesc))
			{
				NUCLEAR_FATAL("[Engine] Failed To Create Window...");
				return failStartup();
			}

			Platform::Input::Get().SetMouseInputMode(Platform::Input::MouseInputMode::Normal);

			auto& modules = GetModuleManager();
			std::vector<std::string> assetDependencies;
			auto registerRuntime = [&](const char* name, std::vector<std::string> dependencies,
				EngineModule& module)
			{
				assetDependencies.emplace_back(name);
				return modules.RegisterModule({ name, ModuleType::Runtime, std::move(dependencies) }, module);
			};
			if (desc.AutoInitGraphicsModule)
			{
				Graphics::GraphicsModuleDesc graphicsDesc;
				graphicsDesc.pWindowHandle = GetMainWindow()->GetSDLWindowPtr();
				Graphics::GraphicsModule::Get().SetStartupDesc(graphicsDesc);
				if (!registerRuntime("Graphics", {}, Graphics::GraphicsModule::Get()))
				{
					NUCLEAR_FATAL("[Engine] Failed to register Graphics module.");
					return failStartup();
				}
			}

			if (desc.AutoInitAudioModule)
			{
				Audio::AudioModuleDesc audioDesc;
				audioDesc.mBackendName = desc.mAudioBackendName;
				audioDesc.mPluginDirectory = desc.mAudioPluginDirectory;
				Audio::AudioModule::Get().SetStartupDesc(audioDesc);
				if (!registerRuntime("Audio", {}, Audio::AudioModule::Get()))
				{
					NUCLEAR_FATAL("[Engine] Failed to register Audio module.");
					return failStartup();
				}
			}
			if (desc.AutoInitScriptingModule)
			{
				Scripting::CSharp::CSharpBackendDesc scriptingDesc;
				scriptingDesc.mScriptingCoreAssemblyDir = std::filesystem::current_path().string();
				scriptingDesc.mClientAssemblyPath = std::filesystem::current_path().string() + "/" + desc.mScriptingClientDllName;
				scriptingDesc.mClientNamespace = desc.mScriptingAssemblyNamespace;
				if (!Scripting::CSharp::RegisterCSharpBackend(std::move(scriptingDesc))) return failStartup();
				Scripting::ScriptingModule::Get().SetStartupDesc({ "CSharp" });
				if (!registerRuntime("Scripting", {}, Scripting::ScriptingModule::Get()))
				{
					NUCLEAR_FATAL("[Engine] Failed to register Scripting module.");
					return failStartup();
				}
			}

			if (desc.AutoInitPhysXModule)
			{
				if (!registerRuntime("Physics", {}, PhysX::PhysXModule::Get()))
				{
					NUCLEAR_FATAL("[Engine] Failed to register Physics module.");
					return failStartup();
				}
			}

			if (desc.AutoInitRenderingModule)
			{
				Rendering::RenderingModuleDesc renderDesc;
				renderDesc.RTWidth = desc.mEngineWindowDesc.WindowWidth;
				renderDesc.RTHeight = desc.mEngineWindowDesc.WindowHeight;
				Rendering::RenderingModule::Get().SetStartupDesc(renderDesc);
				if (!registerRuntime("Rendering", { "Graphics" }, Rendering::RenderingModule::Get()))
				{
					NUCLEAR_FATAL("[Engine] Failed to register Rendering module.");
					return failStartup();
				}			
			}

			if (desc.AutoInitThreadingModule)
			{
				if (!registerRuntime("Threading", {}, Threading::ThreadingModule::Get()))
				{
					NUCLEAR_FATAL("[Engine] Failed to register Threading module.");
					return failStartup();
				}
			}

			if (desc.AutoInitFallbacksModule)
			{
				if (!registerRuntime("Fallbacks", { "Graphics" }, Fallbacks::FallbacksModule::Get()))
				{
					NUCLEAR_FATAL("[Engine] Failed to register Fallbacks module.");
					return failStartup();
				}
			}

			if (!modules.RegisterModule({ "Assets", ModuleType::Runtime, std::move(assetDependencies) },
				std::make_unique<CallbackModule>([]() { Assets::AssetManager::Get().Initialize(); return true; }, []() {})))
				return failStartup();
			if (!desc.mModulePluginDirectory.empty() && !modules.LoadPlugins(desc.mModulePluginDirectory))
			{
				NUCLEAR_FATAL("[Engine] Module plugin loading failed: {0}", modules.GetLastError());
				return failStartup();
			}
			if (!modules.StartModules())
			{
				NUCLEAR_FATAL("[Engine] Module startup failed: {0}", modules.GetLastError());
				return failStartup();
			}
			mThreadingEnabled = desc.AutoInitThreadingModule;
			gisDebug = desc.Debug;

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
			if (pModules) pModules->ShutdownModules();
			pModules.reset();
			mThreadingEnabled = false;
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
		Engine::~Engine() = default;

		ModuleManager& Engine::GetModuleManager()
		{
			if (!pModules) pModules = std::make_unique<ModuleManager>();
			return *pModules;
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
				if (mThreadingEnabled) Threading::ThreadingModule::Get().ExecuteMainThreadTasks(1);
					
				//Render
				Platform::Input::Get().Update();

				// per-frame time logic (ensure speed is constant through all platforms)
				float currentFrame = static_cast<float>(timer.GetElapsedTimeInSeconds());
				pClient->DeltaTime = currentFrame - pClient->LastFrame;
				pClient->LastFrame = currentFrame;
				pClient->ClockTime = static_cast<float>(timer.GetElapsedTimeInSeconds());
				if (pModules) pModules->UpdateModules(pClient->DeltaTime);

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
			NUCLEAR_TRACE("[Engine] Starting Engine...");
			NUCLEAR_TRACE("[Engine] Engine Build: '{0}'.'{1}'  On: '{2}' At: '{3}'", MajorVersion, MinorVersion, __DATE__, __TIME__);

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
