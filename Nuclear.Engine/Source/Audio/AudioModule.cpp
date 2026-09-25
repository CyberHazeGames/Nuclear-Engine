#include <Audio/AudioModule.h>
#include <Audio/AudioBackend.h>
#include <Audio/AudioBackendPlugin.h>
#include <Utilities/Logger.h>
#include <Windows.h>

namespace Nuclear::Audio
{
	AudioModule& AudioModule::Get() { static AudioModule instance; return instance; }
	bool AudioModule::OnInitialize() { return Initialize(mStartupDesc); }
	bool AudioModule::LoadBackend(const AudioModuleDesc& desc)
	{
		if (pBackend || pLibrary) return false;
		std::string name = desc.mBackendName;
		if (name.empty())
		{
			switch (desc.mRequestedBackend)
			{
			case AudioModuleDesc::AudioBackendType::XAudio2: name = "XAudio2"; break;
			case AudioModuleDesc::AudioBackendType::OpenAL:
			case AudioModuleDesc::AudioBackendType::Unknown: name = "OpenAL"; break;
			default: return false;
			}
		}
		std::filesystem::path directory = desc.mPluginDirectory;
		if (directory.empty())
		{
			wchar_t path[32768];
			const auto length = GetModuleFileNameW(nullptr, path, 32768);
			if (!length || length >= 32768) return false;
			directory = std::filesystem::path(path).parent_path();
		}
		const auto libraryPath = std::filesystem::absolute(directory / ("Nuclear.Audio." + name + ".dll"));
		auto library = LoadLibraryW(libraryPath.c_str());
		if (!library)
		{
			NUCLEAR_ERROR("[AudioModule] Failed to load audio plugin: {0} (Win32 {1})", libraryPath.string(), GetLastError());
			return false;
		}
		auto getPlugin = reinterpret_cast<GetAudioBackendPlugin>(GetProcAddress(library, AudioBackendPluginEntryPoint));
		const auto* plugin = getPlugin ? getPlugin() : nullptr;
		if (!plugin || plugin->mSize != sizeof(AudioBackendPlugin) || plugin->mVersion != AudioBackendPluginVersion ||
			!plugin->pName || name != plugin->pName || !plugin->pCreate || !plugin->pDestroy)
		{
			NUCLEAR_ERROR("[AudioModule] Invalid audio plugin: {0}", libraryPath.string());
			FreeLibrary(library);
			return false;
		}
		auto backend = plugin->pCreate();
		if (!backend)
		{
			NUCLEAR_ERROR("[AudioModule] Audio plugin did not create a backend: {0}", name);
			FreeLibrary(library);
			return false;
		}
		pLibrary = library;
		pBackend = backend;
		pDestroyBackend = plugin->pDestroy;
		mBackendName = std::move(name);
		return true;
	}

	bool AudioModule::Initialize(const AudioModuleDesc& desc)
	{
		if (!LoadBackend(desc)) return false;
		if (!pBackend->Initialize())
		{
			NUCLEAR_ERROR("[AudioModule] Backend initialization failed: {0}", mBackendName);
			Shutdown();
			return false;
		}
		mInitialized = true;
		NUCLEAR_INFO("[AudioModule] Initialized {0}.", mBackendName);
		return true;
	}

	AudioBackend* AudioModule::GetBackend() { return pBackend; }

	void AudioModule::Shutdown()
	{
		if (pBackend)
		{
			pBackend->Shutdown();
			pDestroyBackend(pBackend);
			pBackend = nullptr;
		}
		pDestroyBackend = nullptr;
		mInitialized = false;
		mBackendName.clear();
		if (pLibrary)
		{
			FreeLibrary(static_cast<HMODULE>(pLibrary));
			pLibrary = nullptr;
		}
	}

	void AudioModule::Update() {}
}
