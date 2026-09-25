#pragma once
#include <Audio/AudioFile.h>
#include <ECS/Transform.h>

namespace Nuclear
{
	namespace Components
	{
		class AudioSourceComponent;
	}

	namespace Assets
	{
		class AudioClip;
	}
	namespace Audio
	{
		class NEAPI AudioBackend
		{
		public:
			virtual ~AudioBackend() = default;
			virtual bool Initialize() = 0;

			virtual void Shutdown() = 0;

			virtual bool CreateAudioClip(Assets::AudioClip* result, AudioFile& file) = 0;

			virtual bool CreateAudioSource(Components::AudioSourceComponent* result,const ECS::Transform& trans) = 0;;

			virtual void SetAudioSourceClip(const Uint32 source, const Uint32 clip) = 0;;

			virtual void Play(const Uint32 audio_source) = 0;

			virtual void Pause(const Uint32 audio_source) = 0;

			virtual void Stop(const Uint32 audio_source) = 0;

			virtual void SetSource_Volume(const Uint32 audio_source, float vol) = 0;
			virtual void SetSource_Pitch(const Uint32 audio_source, float pitch) = 0;
			//virtual void SetSource_IsLooping(const Uint32 audio_source, bool val) = 0;
			virtual void SetSource_Transform(const Uint32 audio_source, const Math::Vector3& pos, const Math::Quaternion& rot) = 0;
			virtual void SetSource_Velocity(const Uint32 audio_source, const Math::Vector3& val) = 0;


			virtual void SetListener_Velocity(const Math::Vector3& val) = 0;
			virtual void SetListener_Transform( const Math::Vector3& pos, const Math::Quaternion& rot) = 0;
			virtual void Update() = 0;

		};
	}
}
