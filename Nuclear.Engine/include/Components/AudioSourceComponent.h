#pragma once
#include <NE_Common.h>
#include <Math/Math.h>
namespace Nuclear
{
	namespace Assets { class AudioClip; }
	namespace Systems {	class AudioSystem; }
	namespace Components
	{
		/// Scene audio source whose backend handle is created by AudioSystem after a clip is supplied.
		/// Clips are borrowed from the asset library and must outlive the component. Controls are
		/// ignored until a loaded/created clip and a nonzero backend source handle are available.
		class NEAPI AudioSourceComponent
		{
		public:
			enum class State
			{
				Stopped = 0,
				Playing = 1,
				Paused = 2
			};

			AudioSourceComponent();
			AudioSourceComponent(Assets::AudioClip* clip);
			~AudioSourceComponent();

			FORCE_INLINE Assets::AudioClip* GetAudioClip()
			{
				return pActiveClip;
			}
			/// Assigns a borrowed clip (nullptr is allowed); binds ready clips only to an existing source.
			/// Assigning a clip does not create a backend source; AudioSystem handles source creation.
			void SetAudioClip(Assets::AudioClip* clip);

			FORCE_INLINE Uint32 GetSourceID() const
			{
				return mSourceID;
			}

			void SetVolume(float vol);
			void SetPitch(float pitch);
			void SetIsLooping(bool val);

			FORCE_INLINE float GetVolume() const
			{
				return mVolume;
			}
			FORCE_INLINE float GetPitch() const
			{
				return mPitch;
			}
			FORCE_INLINE bool  GetLoop() const
			{
				return mLoop;
			}

			FORCE_INLINE State GetState() const
			{
				return mState;
			}

			void Play();
			void Pause();
			void Stop();

		//protected:
			friend class Systems::AudioSystem;
			State mState;
			Uint32 mSourceID;					//Openal = source id , XAudio index in sources vector

			Math::Vector3 mVelocity;
			Math::Vector3 mPrevPos;
		private:
			float mVolume;   //Note: [0, 1] range
			float mPitch;
			bool mLoop;
			Assets::AudioClip* pActiveClip;
		};
	}
}
