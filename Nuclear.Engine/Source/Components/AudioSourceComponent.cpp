#include "Components/AudioSourceComponent.h"
#include <Audio/AudioModule.h>
#include <Audio/AudioBackend.h>
#include <Assets/AudioClip.h>
namespace Nuclear
{
	namespace Components
	{
		namespace
		{
			Audio::AudioBackend* GetReadyBackend(AudioSourceComponent& source)
			{
				auto* clip = source.GetAudioClip();
				if (!clip || source.GetSourceID() == 0 || clip->GetBufferID() == 0)
				{
					return nullptr;
				}

				const auto state = clip->GetState();
				if (state != Assets::IAsset::State::Loaded && state != Assets::IAsset::State::Created)
				{
					return nullptr;
				}

				return Audio::AudioModule::Get().GetBackend();
			}
		}

		AudioSourceComponent::AudioSourceComponent()
			: AudioSourceComponent(nullptr)
		{

		}
		AudioSourceComponent::AudioSourceComponent(Assets::AudioClip* clip)
			: pActiveClip(clip), mSourceID(0), mVolume(1.0f), mPitch(1.0f), mLoop(false), mState(AudioSourceComponent::State::Stopped), mPrevPos(0.0f), mVelocity(0.0f)
		{
		}
		AudioSourceComponent::~AudioSourceComponent()
		{
		}
		void AudioSourceComponent::SetAudioClip(Assets::AudioClip* clip)
		{
			pActiveClip = clip;

			if (auto* backend = GetReadyBackend(*this))
			{
				backend->SetAudioSourceClip(mSourceID, pActiveClip->GetBufferID());
			}
		}
		void AudioSourceComponent::SetVolume(float vol)
		{
			mVolume = vol;
			if (auto* backend = GetReadyBackend(*this))
			{
				backend->SetSource_Volume(mSourceID, vol);
			}
		}
		void AudioSourceComponent::SetPitch(float pitch)
		{
			mPitch = pitch;
			if (auto* backend = GetReadyBackend(*this))
			{
				backend->SetSource_Pitch(mSourceID, pitch);
			}
		}
		void AudioSourceComponent::SetIsLooping(bool val)
		{
			mLoop = val;
			// Backend looping is configured from the clip when its source is created.
		}
		void AudioSourceComponent::Play()
		{
			if (auto* backend = GetReadyBackend(*this))
			{
				backend->Play(mSourceID);
			}
		}
		void AudioSourceComponent::Pause()
		{
			if (auto* backend = GetReadyBackend(*this))
			{
				backend->Pause(mSourceID);
			}
		}
		void AudioSourceComponent::Stop()
		{
			if (auto* backend = GetReadyBackend(*this))
			{
				backend->Stop(mSourceID);
			}
		}
	}
}
