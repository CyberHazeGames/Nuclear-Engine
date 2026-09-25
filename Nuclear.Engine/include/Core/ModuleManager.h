#pragma once
#include <NE_Common.h>
#include <Core/EngineModule.h>
#include <Core/EngineModulePlugin.h>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <utility>

namespace Nuclear::Core
{
	enum class ModuleType { Runtime, Editor, Developer, Program };

	struct ModuleDescriptor
	{
		std::string mName;
		ModuleType mType = ModuleType::Runtime;
		std::vector<std::string> mDependencies;
	};

	// Handles services that are not EngineModule subclasses yet.
	class CallbackModule final : public EngineModule
	{
	public:
		CallbackModule(std::function<bool()> initialize, std::function<void()> shutdown)
			: mInitialize(std::move(initialize)), mShutdown(std::move(shutdown)) {}
		bool OnInitialize() override { return mInitialize ? mInitialize() : true; }
		void Shutdown() override { if (mShutdown) mShutdown(); }
	private:
		std::function<bool()> mInitialize;
		std::function<void()> mShutdown;
	};

	class NEAPI ModuleManager
	{
	public:
		ModuleManager() = default;
		~ModuleManager();
		ModuleManager(const ModuleManager&) = delete;
		ModuleManager& operator=(const ModuleManager&) = delete;
		bool RegisterModule(ModuleDescriptor descriptor, std::unique_ptr<EngineModule> module);
		bool RegisterModule(ModuleDescriptor descriptor, EngineModule& module);
		bool LoadPlugin(const std::filesystem::path& libraryPath);
		bool LoadPlugins(const std::filesystem::path& directory);
		bool StartModules();
		void UpdateModules(float deltaTime);
		void ShutdownModules();
		const std::string& GetLastError() const { return mLastError; }
		std::vector<std::string> GetResolvedOrder() const;
	private:
		enum class State { Unloaded, Loaded, Initialized, Started };
		struct Entry
		{
			ModuleDescriptor mDescriptor;
			std::unique_ptr<EngineModule> pOwnedModule;
			EngineModule* pModule = nullptr;
			void (*pPluginDestroy)(EngineModule*) = nullptr;
			void* pPluginLibrary = nullptr;
			State mState = State::Unloaded;
		};
		bool ResolveOrder();
		void UnloadPluginsFrom(size_t firstPlugin);
		std::vector<Entry> mEntries;
		std::vector<void*> mPluginLibraries;
		std::vector<size_t> mOrder;
		std::string mLastError;
		bool mRunning = false;
	};
}
