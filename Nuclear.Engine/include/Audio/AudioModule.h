#pragma once
#include <NE_Common.h>
#include <Core/EngineModule.h>
#include <filesystem>
#include <string>

namespace Nuclear::Audio
{
	class AudioBackend;

	struct AudioModuleDesc
	{
		enum class AudioBackendType : Uint8
		{
			Unknown,
			OpenAL = 1,
			XAudio2 = 2,
			MAX_UNKNOWN = UINT8_MAX
		};
		AudioBackendType mRequestedBackend = AudioBackendType::Unknown;
		std::string mBackendName;
		std::filesystem::path mPluginDirectory;
		unsigned int MaxChannels = 32;
	};

	class NEAPI AudioModule : public Core::EngineModule
	{
	public:
		static AudioModule& Get();
		void SetStartupDesc(const AudioModuleDesc& desc) { mStartupDesc = desc; }
		bool OnInitialize() override;
		AudioBackend* GetBackend();
		const std::string& GetBackendName() const { return mBackendName; }
		bool LoadBackend(const AudioModuleDesc& desc);
		bool Initialize(const AudioModuleDesc& desc);
		bool IsInitialized() const { return mInitialized; }
		void Shutdown() override;
		void Update();
	private:
		AudioModule() = default;
		AudioModuleDesc mStartupDesc{};
		AudioBackend* pBackend = nullptr;
		void (*pDestroyBackend)(AudioBackend*) = nullptr;
		void* pLibrary = nullptr;
		std::string mBackendName;
		bool mInitialized = false;
	};
}
