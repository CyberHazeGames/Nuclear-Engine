#include <Audio/AudioBackendPlugin.h>
#include "OpenALBackend.h"

namespace
{
	Nuclear::Audio::AudioBackend* Create() { return new Nuclear::Audio::OpenALBackend(); }
	void Destroy(Nuclear::Audio::AudioBackend* backend) { delete backend; }
	const Nuclear::Audio::AudioBackendPlugin Plugin = {
		sizeof(Nuclear::Audio::AudioBackendPlugin), Nuclear::Audio::AudioBackendPluginVersion,
		"OpenAL", &Create, &Destroy
	};
}

extern "C" __declspec(dllexport) const Nuclear::Audio::AudioBackendPlugin* GetNuclearAudioBackendPlugin()
{
	return &Plugin;
}
