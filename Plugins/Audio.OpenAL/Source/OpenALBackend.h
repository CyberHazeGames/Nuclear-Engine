#pragma once
#include <Audio/AudioBackend.h>

struct ALCdevice;
struct ALCcontext;

namespace Nuclear
{
	namespace Audio
	{

		class OpenALBackend : public AudioBackend
		{
		public:
			OpenALBackend();
			~OpenALBackend();

			bool Initialize() override;

			void Shutdown() override;

			bool CreateAudioClip(Assets::AudioClip* result, AudioFile& file) override;

			bool CreateAudioSource(Components::AudioSourceComponent* source, const ECS::Transform& trans) override;

			void SetAudioSourceClip(const Uint32 source, const Uint32 clip) override;

			void Play(const Uint32 clip) override;

			void Pause(const Uint32 clip) override;

			void Stop(const Uint32 clip) override;

			void SetSource_Volume(const Uint32 audio_source, float vol) override;

			void SetSource_Pitch(const Uint32 audio_source, float pitch) override;

			//void SetSource_IsLooping(const Uint32 audio_source, bool val) override;

			void SetSource_Transform(const Uint32 audio_source, const Math::Vector3& pos, const Math::Quaternion& rot) override;

			void SetSource_Velocity(const Uint32 audio_source, const Math::Vector3& val) override;

			void SetListener_Velocity(const Math::Vector3& val) override;

			void SetListener_Transform(const Math::Vector3& pos, const Math::Quaternion& orientation) override;

			void Update() override;
		protected:
			ALCdevice* pDevice;
			ALCcontext* pMasterContext;
		};
	}
}
