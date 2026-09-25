#include <Core/ModuleManager.h>
#include <Windows.h>
#include <algorithm>
#include <unordered_map>

namespace Nuclear::Core
{
	ModuleManager::~ModuleManager() { ShutdownModules(); }

	bool ModuleManager::RegisterModule(ModuleDescriptor descriptor, std::unique_ptr<EngineModule> module)
	{
		if (mRunning || !module || descriptor.mName.empty()) return false;
		for (const auto& entry : mEntries)
			if (entry.mDescriptor.mName == descriptor.mName) return false;
		auto* instance = module.get();
		mEntries.push_back({ std::move(descriptor), std::move(module), instance });
		mOrder.clear();
		return true;
	}

	bool ModuleManager::RegisterModule(ModuleDescriptor descriptor, EngineModule& module)
	{
		if (mRunning || descriptor.mName.empty()) return false;
		for (const auto& entry : mEntries)
			if (entry.mDescriptor.mName == descriptor.mName || entry.pModule == &module) return false;
		mEntries.push_back({ std::move(descriptor), {}, &module });
		mOrder.clear();
		return true;
	}

	bool ModuleManager::LoadPlugin(const std::filesystem::path& libraryPath)
	{
		if (mRunning || libraryPath.empty())
		{
			mLastError = "Plugins must be loaded before module startup";
			return false;
		}
		std::error_code error;
		const auto path = std::filesystem::absolute(libraryPath, error);
		if (error)
		{
			mLastError = "Invalid module plugin path: " + libraryPath.string();
			return false;
		}
		auto library = LoadLibraryExW(path.c_str(), nullptr,
			LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
		if (!library)
		{
			mLastError = "Failed to load module plugin " + path.string() + " (Win32 " + std::to_string(::GetLastError()) + ")";
			return false;
		}
		auto entryPoint = reinterpret_cast<GetEngineModulePlugin>(GetProcAddress(library, EngineModulePluginEntryPoint));
		const auto* plugin = entryPoint ? entryPoint() : nullptr;
		std::vector<Entry> pending;
		auto fail = [&](std::string message)
		{
			mLastError = std::move(message);
			for (auto it = pending.rbegin(); it != pending.rend(); ++it)
				it->pPluginDestroy(it->pModule);
			FreeLibrary(library);
			return false;
		};
		if (!plugin || plugin->mSize != sizeof(EngineModulePlugin) ||
			plugin->mVersion != EngineModulePluginVersion || !plugin->pModules ||
			plugin->mModuleCount == 0 || plugin->mModuleCount > 256)
			return fail("Invalid module plugin ABI: " + path.string());
		pending.reserve(plugin->mModuleCount);
		for (std::uint32_t i = 0; i < plugin->mModuleCount; ++i)
		{
			const auto& factory = plugin->pModules[i];
			if (!factory.pName || !*factory.pName || factory.mType > static_cast<std::uint32_t>(ModuleType::Program) ||
				factory.mDependencyCount > 256 || (factory.mDependencyCount && !factory.pDependencies) ||
				!factory.pCreate || !factory.pDestroy)
				return fail("Invalid module factory in " + path.string());
			ModuleDescriptor descriptor;
			descriptor.mName = factory.pName;
			descriptor.mType = static_cast<ModuleType>(factory.mType);
			for (std::uint32_t j = 0; j < factory.mDependencyCount; ++j)
			{
				if (!factory.pDependencies[j] || !*factory.pDependencies[j])
					return fail("Invalid module dependency in " + path.string());
				descriptor.mDependencies.emplace_back(factory.pDependencies[j]);
			}
			for (const auto& entry : mEntries)
				if (entry.mDescriptor.mName == descriptor.mName)
					return fail("Duplicate module name: " + descriptor.mName);
			for (const auto& entry : pending)
				if (entry.mDescriptor.mName == descriptor.mName)
					return fail("Duplicate module name: " + descriptor.mName);
			auto* module = factory.pCreate();
			if (!module) return fail("Module factory failed: " + descriptor.mName);
			Entry entry;
			entry.mDescriptor = std::move(descriptor);
			entry.pModule = module;
			entry.pPluginDestroy = factory.pDestroy;
			entry.pPluginLibrary = library;
			pending.push_back(std::move(entry));
		}
		for (auto& entry : pending) mEntries.push_back(std::move(entry));
		mPluginLibraries.push_back(library);
		mOrder.clear();
		mLastError.clear();
		return true;
	}

	bool ModuleManager::LoadPlugins(const std::filesystem::path& directory)
	{
		if (mRunning || directory.empty())
		{
			mLastError = "A module plugin directory is required before startup";
			return false;
		}
		std::error_code error;
		std::vector<std::filesystem::path> paths;
		for (std::filesystem::directory_iterator it(directory, error), end; !error && it != end; it.increment(error))
		{
			const auto filename = it->path().filename().wstring();
			if (it->is_regular_file(error) && filename.starts_with(L"Nuclear.Module.") && it->path().extension() == L".dll")
				paths.push_back(it->path());
		}
		if (error)
		{
			mLastError = "Cannot enumerate module plugins in " + directory.string() + ": " + error.message();
			return false;
		}
		std::sort(paths.begin(), paths.end());
		const auto firstPlugin = mPluginLibraries.size();
		for (const auto& path : paths)
			if (!LoadPlugin(path))
			{
				UnloadPluginsFrom(firstPlugin);
				return false;
			}
		mLastError.clear();
		return true;
	}

	void ModuleManager::UnloadPluginsFrom(size_t firstPlugin)
	{
		if (firstPlugin >= mPluginLibraries.size()) return;
		auto belongsToSelection = [&](const Entry& entry)
		{
			return entry.pPluginLibrary &&
				std::find(mPluginLibraries.begin() + firstPlugin, mPluginLibraries.end(), entry.pPluginLibrary) != mPluginLibraries.end();
		};
		for (auto it = mOrder.rbegin(); it != mOrder.rend(); ++it)
		{
			auto& entry = mEntries[*it];
			if (belongsToSelection(entry) && entry.pModule)
			{
				entry.pPluginDestroy(entry.pModule);
				entry.pModule = nullptr;
			}
		}
		for (auto it = mEntries.rbegin(); it != mEntries.rend(); ++it)
			if (belongsToSelection(*it) && it->pModule) it->pPluginDestroy(it->pModule);
		std::erase_if(mEntries, belongsToSelection);
		for (size_t i = mPluginLibraries.size(); i > firstPlugin; --i)
			FreeLibrary(static_cast<HMODULE>(mPluginLibraries[i - 1]));
		mPluginLibraries.resize(firstPlugin);
		mOrder.clear();
	}

	bool ModuleManager::ResolveOrder()
	{
		mOrder.clear();
		mLastError.clear();
		std::unordered_map<std::string, size_t> names;
		for (size_t i = 0; i < mEntries.size(); ++i)
			names.emplace(mEntries[i].mDescriptor.mName, i);
		std::vector<int> states(mEntries.size(), 0);
		std::function<bool(size_t)> visit = [&](size_t index)
		{
			if (states[index] == 2) return true;
			if (states[index] == 1)
			{
				mLastError = "Module dependency cycle at " + mEntries[index].mDescriptor.mName;
				return false;
			}
			states[index] = 1;
			for (const auto& dependency : mEntries[index].mDescriptor.mDependencies)
			{
				auto found = names.find(dependency);
				if (found == names.end())
				{
					mLastError = "Missing module dependency " + dependency + " for " + mEntries[index].mDescriptor.mName;
					return false;
				}
				if (!visit(found->second)) return false;
			}
			states[index] = 2;
			mOrder.push_back(index);
			return true;
		};
		for (size_t i = 0; i < mEntries.size(); ++i)
			if (!visit(i)) { mOrder.clear(); return false; }
		return true;
	}

	bool ModuleManager::StartModules()
	{
		if (mRunning) return false;
		if (!ResolveOrder()) return false;
		for (size_t index : mOrder)
		{
			auto& entry = mEntries[index];
			if (!entry.pModule->OnLoad())
			{
				mLastError = "Failed to load module " + entry.mDescriptor.mName;
				ShutdownModules();
				return false;
			}
			entry.mState = State::Loaded;
		}
		for (size_t index : mOrder)
		{
			auto& entry = mEntries[index];
			if (!entry.pModule->OnInitialize())
			{
				mLastError = "Failed to initialize module " + entry.mDescriptor.mName;
				ShutdownModules();
				return false;
			}
			entry.mState = State::Initialized;
		}
		for (size_t index : mOrder)
		{
			auto& entry = mEntries[index];
			if (!entry.pModule->OnStart())
			{
				mLastError = "Failed to start module " + entry.mDescriptor.mName;
				ShutdownModules();
				return false;
			}
			entry.mState = State::Started;
		}
		mRunning = true;
		return true;
	}

	void ModuleManager::UpdateModules(float deltaTime)
	{
		if (!mRunning) return;
		for (size_t index : mOrder) mEntries[index].pModule->OnUpdate(deltaTime);
	}

	void ModuleManager::ShutdownModules()
	{
		for (auto it = mOrder.rbegin(); it != mOrder.rend(); ++it)
			if (mEntries[*it].mState == State::Started) mEntries[*it].pModule->OnStop();
		for (auto it = mOrder.rbegin(); it != mOrder.rend(); ++it)
			if (mEntries[*it].mState >= State::Initialized) mEntries[*it].pModule->Shutdown();
		for (auto it = mOrder.rbegin(); it != mOrder.rend(); ++it)
		{
			if (mEntries[*it].mState >= State::Loaded) mEntries[*it].pModule->OnUnload();
			mEntries[*it].mState = State::Unloaded;
		}
		mRunning = false;
		UnloadPluginsFrom(0);
	}

	std::vector<std::string> ModuleManager::GetResolvedOrder() const
	{
		std::vector<std::string> names;
		for (size_t index : mOrder) names.push_back(mEntries[index].mDescriptor.mName);
		return names;
	}
}
