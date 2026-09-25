#pragma once
#include <cstdint>

namespace Nuclear::Audio
{
	class AudioBackend;
	inline constexpr std::uint32_t AudioBackendPluginVersion = 1;
	inline constexpr const char* AudioBackendPluginEntryPoint = "GetNuclearAudioBackendPlugin";

	struct AudioBackendPlugin
	{
		std::uint32_t mSize;
		std::uint32_t mVersion;
		const char* pName;
		AudioBackend* (*pCreate)();
		void (*pDestroy)(AudioBackend*);
	};

	using GetAudioBackendPlugin = const AudioBackendPlugin* (*)();
}
