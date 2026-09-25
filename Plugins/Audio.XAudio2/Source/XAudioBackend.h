#pragma once
#include <Audio/AudioBackend.h>
#include <xaudio2.h>
#include <x3daudio.h>

#define VEC_CAST(vec) (*((X3DAUDIO_VECTOR*)&vec))

namespace Nuclear
{
	namespace Audio
	{

		struct XAudioSource
		{
			IXAudio2SourceVoice* pVoice = nullptr;
			X3DAUDIO_EMITTER m3DEmitter = { 0 };
			WAVEFORMATEX mFormat = { 0 };
			float mPitch = 1.0f;
			bool mDirty = false;
			bool mIs3D = false;

			FORCE_INLINE void SetTransform(const Math::Vector3& pos, const Math::Quaternion& rot)
			{
				const Math::Vector3 front = rot * Math::Vector3(0.0f, 0.0f, 1.0f); // Forward
				const Math::Vector3 top = rot * Math::Vector3(0.0f, 1.0f, 0.0f);    // Up

				m3DEmitter.OrientFront = VEC_CAST(front);
				m3DEmitter.OrientTop = VEC_CAST(top);
				m3DEmitter.Position = VEC_CAST(pos);

				mDirty = true;
			}

			FORCE_INLINE void SetVelocity(const Math::Vector3& val)
			{
			//	m3DEmitter.Velocity = VEC_CAST(val);

				mDirty = true;
			}

			FORCE_INLINE bool Unused() const
			{
				return (pVoice == nullptr);
			}
		};

		struct XAudioBuffer
		{
			AudioFile mFile;
			bool Used = false;
			bool mIs3D = false;
			bool mLoop = false;
		};

		class XAudioBackend : public AudioBackend
		{
		public:
			XAudioBackend();
			~XAudioBackend();

			bool Initialize() override;

			void Shutdown() override;
			bool CreateAudioClip(Assets::AudioClip* result, AudioFile& file) override;

			bool CreateAudioSource(Components::AudioSourceComponent* source, const ECS::Transform& trans) override;

			void SetAudioSourceClip(const Uint32 source, const Uint32 clip) override;

			void Play(const Uint32 source) override;

			void Pause(const Uint32 source) override;

			void Stop(const Uint32 source) override;

			void SetSource_Volume(const Uint32 audio_source, float vol) override;

			void SetSource_Pitch(const Uint32 audio_source, float pitch) override;

			//void SetSource_IsLooping(const Uint32 audio_source, bool val) override;

			void SetSource_Transform(const Uint32 audio_source, const Math::Vector3& pos, const Math::Quaternion& rot) override;

			void SetSource_Velocity(const Uint32 audio_source, const Math::Vector3& val) override;

			void SetListener_Velocity(const Math::Vector3& val) override;

			void SetListener_Transform(const Math::Vector3& pos, const Math::Quaternion& rot) override;

			void Update() override;

		protected:
			XAudioSource& GetSource(const Uint32 id);
			XAudioBuffer& GetBuffer(const Uint32 id);

			XAudioSource& GetUnusedSource(Uint32& id);
			XAudioBuffer& GetUnusedBuffer(Uint32& id);

			XAudioSource* GetSourcePtr(const Uint32 audio_source);
			//XAudioBuffer* GetBufferFromSource(const Uint32 audio_source);
			//XAudioBuffer* GetBufferPtr(const Uint32 clip);

			UINT32 mChannels = 0;
			FLOAT32* pMatrixCoefficients = nullptr;
			IXAudio2* pXAudio2 = nullptr;
			IXAudio2MasteringVoice* pMasterVoice = nullptr;
			X3DAUDIO_HANDLE X3DInstance;
			X3DAUDIO_LISTENER m3DListener;

			bool mListenerDirty = false;
			std::vector<XAudioSource> mSources;
			std::vector<XAudioBuffer> mBuffers;
		};
	}
}
