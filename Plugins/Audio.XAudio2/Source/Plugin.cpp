#include <Audio/AudioBackendPlugin.h>
#include "XAudioBackend.h"

namespace
{
	Nuclear::Audio::AudioBackend* Create() { return new Nuclear::Audio::XAudioBackend(); }
	void Destroy(Nuclear::Audio::AudioBackend* backend) { delete backend; }
	const Nuclear::Audio::AudioBackendPlugin Plugin = {
		sizeof(Nuclear::Audio::AudioBackendPlugin), Nuclear::Audio::AudioBackendPluginVersion,
		"XAudio2", &Create, &Destroy
	};
}

extern "C" __declspec(dllexport) const Nuclear::Audio::AudioBackendPlugin* GetNuclearAudioBackendPlugin()
{
	return &Plugin;
}
