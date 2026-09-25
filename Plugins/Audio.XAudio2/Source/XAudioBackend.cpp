#include "XAudioBackend.h"
#include <Utilities/Logger.h>
#include <Components/AudioSourceComponent.h>
#include <Assets/AudioClip.h>


#pragma comment(lib,"XAudio2.lib")

namespace Nuclear
{
	namespace Audio
	{
		class XAudioCallback : public IXAudio2EngineCallback
		{
			COM_DECLSPEC_NOTHROW void OnProcessingPassEnd() override
			{
				return;
			}

			COM_DECLSPEC_NOTHROW void OnProcessingPassStart() override
			{
				return;
			}

			COM_DECLSPEC_NOTHROW void OnCriticalError(HRESULT error) override
			{
				NUCLEAR_ERROR("[XAudioBackend] XAudio2 Critical Error! Code: 0x{0:x}", error);
			}
		};

		XAudioBackend::XAudioBackend()
		{
			pXAudio2 = nullptr;
			pMasterVoice = nullptr;
			m3DListener = { };
		}

		XAudioBackend::~XAudioBackend()
		{
			pXAudio2 = nullptr;
			pMasterVoice = nullptr;
		}

		bool XAudioBackend::Initialize()
		{
			HRESULT hr = XAudio2Create(&pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
			if (FAILED(hr))
			{
				NUCLEAR_ERROR("[XAudioBackend] Failed to initalize XAudio2! Error Code: 0x{0:x}", hr);
				return false;
			}
			
			static XAudioCallback callbacks;

			pXAudio2->RegisterForCallbacks(&callbacks);

			hr = pXAudio2->CreateMasteringVoice(&pMasterVoice);

			if (FAILED(hr))
			{
				NUCLEAR_ERROR("[XAudioBackend] Failed to Create XAudio2 MasterVoice! Error Code: 0x{0:x}", hr);
				return false;
			}

			XAUDIO2_VOICE_DETAILS details = { 0 };
			pMasterVoice->GetVoiceDetails(&details);
			mChannels = details.InputChannels;
			pMatrixCoefficients = new FLOAT32[8];

			DWORD dwChannelMask;
			pMasterVoice->GetChannelMask(&dwChannelMask);
			hr = X3DAudioInitialize(dwChannelMask, X3DAUDIO_SPEED_OF_SOUND, X3DInstance);
			if (FAILED(hr))
			{
				NUCLEAR_ERROR("[XAudioBackend] Failed to initalize X3DAudio! Error Code: 0x{0:x}", hr);
				return false;
			}
			
			//TODO: this number should be modifiable in initialization;
			mSources.reserve(8);
			mBuffers.reserve(8);

			NUCLEAR_TRACE("[XAudioBackend] Initialized XAudio2 successfully.");
			return true;
		}
		void XAudioBackend::Shutdown()
		{
			if (pMasterVoice)
			{
				pMasterVoice->DestroyVoice();
				pMasterVoice = nullptr;
			}

			if (pXAudio2)
			{
				pXAudio2->StopEngine();
				pXAudio2->Release();
				pXAudio2 = nullptr;
			}

			if (pMatrixCoefficients)
			{
				delete[] pMatrixCoefficients;
				pMatrixCoefficients = nullptr;
			}
		}
		bool XAudioBackend::CreateAudioClip(Assets::AudioClip* result, AudioFile& file)
		{
			auto& buffer = GetUnusedBuffer(result->mBufferID);
			buffer.mFile.mData = std::move(file.mData);
			buffer.mFile.mInfo = file.mInfo;
			buffer.mIs3D = result->m3D;
			buffer.mLoop = result->mLoop;
			buffer.Used = true;
			return true;
		}

		bool XAudioBackend::CreateAudioSource(Components::AudioSourceComponent* result, const ECS::Transform& trans)
		{
			auto& xbuffer = GetBuffer(result->GetAudioClip()->GetBufferID());
			if (!xbuffer.Used)
			{
				return false;
			}

			
			WAVEFORMATEX  waveFormat = { 0 };
			waveFormat.wFormatTag = WAVE_FORMAT_PCM;
			waveFormat.nChannels = xbuffer.mFile.mInfo.mChannels;
			waveFormat.nSamplesPerSec = xbuffer.mFile.mInfo.mSampleRate;
			waveFormat.wBitsPerSample = xbuffer.mFile.mInfo.mBitDepth;
			waveFormat.nBlockAlign = waveFormat.nChannels *	waveFormat.wBitsPerSample / 8;
			waveFormat.nAvgBytesPerSec =waveFormat.nSamplesPerSec *	waveFormat.nBlockAlign;
			waveFormat.cbSize = sizeof(WAVEFORMATEX);

			IXAudio2SourceVoice* voice;
			auto hr = pXAudio2->CreateSourceVoice(&voice, &waveFormat);
			
			if (FAILED(hr))
			{
				NUCLEAR_ERROR("[XAudioBackend] Failed to create AudioSourceComponent!, Error Code:  0x{0:x}", hr);
				return false;
			}

			X3DAUDIO_EMITTER emitter = {};
			if (result->GetAudioClip()->Is3D())
			{
				emitter.ChannelCount = xbuffer.mFile.mInfo.mChannels;
				emitter.CurveDistanceScaler = emitter.DopplerScaler = 1.0f;
				emitter.InnerRadius = 1.0f;

				if (emitter.ChannelCount > 1)
				{
					NUCLEAR_WARN("[XAudioBackend] 3D AudioClips should be mono channeled!");
				}

				static X3DAUDIO_DISTANCE_CURVE_POINT DefaultCurvePoints[2] = { 0.0f, 1.0f, 1.0f, 1.0f };
				static X3DAUDIO_DISTANCE_CURVE       DefaultCurve = { (X3DAUDIO_DISTANCE_CURVE_POINT*)&DefaultCurvePoints[0], 2 };

				if (emitter.pVolumeCurve == NULL)
					emitter.pVolumeCurve = &DefaultCurve;
				
				if (emitter.pLFECurve == NULL)				
					emitter.pLFECurve = &DefaultCurve;			

			}

			auto& source = GetUnusedSource(result->mSourceID);
			source.pVoice = voice;
			source.m3DEmitter = emitter;
			source.mFormat = waveFormat;
			source.mIs3D = result->GetAudioClip()->Is3D();
			source.SetTransform(trans.GetLocalPosition(), trans.GetLocalRotation());
			source.SetVelocity(Math::Vector3(0.0f));

			if (result->GetAudioClip())
			{
				SetAudioSourceClip(result->mSourceID, result->GetAudioClip()->GetBufferID());
			}

			return true;
		}

		void XAudioBackend::SetAudioSourceClip(const Uint32 source, const Uint32 clip)
		{
			auto& xsource = GetSource(source);
			if (xsource.Unused())
			{
				return;
			}

			auto& xbuffer = GetBuffer(clip);
			if (!xbuffer.Used)
			{
				return;
			}

			XAUDIO2_BUFFER buffer = { 0 };
			buffer.AudioBytes = (UINT32)xbuffer.mFile.mData.size();
			buffer.Flags = XAUDIO2_END_OF_STREAM;
			buffer.LoopBegin = 0;
			buffer.LoopCount = xbuffer.mLoop ? XAUDIO2_LOOP_INFINITE : 0;
			buffer.LoopLength = 0;
			buffer.pAudioData = xbuffer.mFile.mData.data();
			buffer.pContext = nullptr;
			buffer.PlayBegin = 0;
			buffer.PlayLength = (UINT32)xbuffer.mFile.mInfo.mSamples;
			
			auto hr = xsource.pVoice->SubmitSourceBuffer(&buffer);

			if (FAILED(hr))
			{
				NUCLEAR_ERROR("[XAudioBackend] Failed to SubmitSourceBuffer!, Error Code:  0x{0:x}", hr);
			}

			return;
		}

		void XAudioBackend::Play(const Uint32 source)
		{
			GetSource(source).pVoice->Start(0);
		}
		void XAudioBackend::Pause(const Uint32 source)
		{
			GetSource(source).pVoice->Stop(0);
		}
		void XAudioBackend::Stop(const Uint32 source)
		{
		}
		void XAudioBackend::SetSource_Volume(const Uint32 audio_source, float vol)
		{
			auto source = GetSourcePtr(audio_source);
			if (source && !source->Unused())
			{
				source->pVoice->SetVolume(vol);
			}
		}
		void XAudioBackend::SetSource_Pitch(const Uint32 audio_source, float pitch)
		{
			auto source = GetSourcePtr(audio_source);
			if (source && !source->Unused())
			{
				//source->pVoice->SetFrequencyRatio(pitch);
				source->mPitch = pitch;
				source->mDirty = true;
			}
		}
	//	void XAudioBackend::SetSource_IsLooping(const Uint32 audio_source, bool val)
	//	{
			
			//Looping in xaudio is through buffers not source
			//possible solutions:
			//-maybe we can force source to replay after it finished playing?
			//-re-creating a buffer for the source 
		//}

		void XAudioBackend::SetSource_Transform(const Uint32 audio_source, const Math::Vector3& pos, const Math::Quaternion& rot)
		{
			auto source = GetSourcePtr(audio_source);
			if (source && !source->Unused())
			{
				source->SetTransform(pos, rot);
			}
		}
		void XAudioBackend::SetSource_Velocity(const Uint32 audio_source, const Math::Vector3& val)
		{
			auto source = GetSourcePtr(audio_source);
			if (source && !source->Unused())
			{
			//	source->SetVelocity(Math::Vector3(0.0f, 0.0f, 0.0f));
			}
		}
		void XAudioBackend::SetListener_Velocity(const Math::Vector3& val)
		{
			//m3DListener.Velocity = VEC_CAST(val);
			//mListenerDirty = true;

		}
		void XAudioBackend::SetListener_Transform(const Math::Vector3& pos, const Math::Quaternion& rot)
		{
			const Math::Vector3 front = rot * Math::Vector3(0.0f, 0.0f, 1.0f); // Forward
			const Math::Vector3 top = rot * Math::Vector3(0.0f, 1.0f, 0.0f);    // Up

			m3DListener.OrientFront = VEC_CAST(front);
			m3DListener.OrientTop = VEC_CAST(top);
			m3DListener.Position = VEC_CAST(pos);
			mListenerDirty = true;
		}

		void XAudioBackend::Update()
		{
			//TODO
			X3DAUDIO_DSP_SETTINGS DSPSettings = { 0 };
			DSPSettings.DstChannelCount = mChannels;
			DSPSettings.pMatrixCoefficients = pMatrixCoefficients;

			for (Uint32 i = 0; i < mSources.size(); i++)
			{
				auto& source = mSources[i];
								
				if (!source.Unused())
				{
					if (mListenerDirty || source.mDirty)
					{
						DSPSettings.SrcChannelCount = source.m3DEmitter.ChannelCount;

						if (source.mIs3D)
						{
							X3DAudioCalculate(X3DInstance, &m3DListener, &source.m3DEmitter, X3DAUDIO_CALCULATE_MATRIX | X3DAUDIO_CALCULATE_DOPPLER, &DSPSettings);

							source.pVoice->SetOutputMatrix(pMasterVoice, DSPSettings.SrcChannelCount, DSPSettings.DstChannelCount, DSPSettings.pMatrixCoefficients);
							float frequency_ratio = source.mPitch * DSPSettings.DopplerFactor;
							source.pVoice->SetFrequencyRatio(frequency_ratio);
						}
						else
						{
							source.pVoice->SetFrequencyRatio(source.mPitch);
						}

						mListenerDirty = false;
						source.mDirty = false;
					}
				}						
			}
		}

		XAudioSource& XAudioBackend::GetSource(const Uint32 id)
		{
			if (id > mSources.size())
			{
				mSources.push_back(XAudioSource());
				return mSources.back();
			}
			return mSources[id - 1];			
		}
		XAudioBuffer& XAudioBackend::GetBuffer(const Uint32 id)
		{
			if (id > mBuffers.size())
			{
				mBuffers.push_back(XAudioBuffer());
				return mBuffers.back();
			}

			return mBuffers[id - 1];
		}
		XAudioSource& XAudioBackend::GetUnusedSource(Uint32& id)
		{
			for (Uint32 i = 1; i < mSources.size(); i++)
			{
				auto& source = mSources[i];
				if (source.Unused())
				{
					id = i;
					return source;
				}
			}

			mSources.push_back(XAudioSource());
			id = mSources.size();
			return mSources.back();
		}
		XAudioBuffer& XAudioBackend::GetUnusedBuffer(Uint32& id)
		{
			for (Uint32 i = 1; i < mBuffers.size(); i++)
			{
				auto& buffer = mBuffers[i];
				if (!buffer.Used)
				{
					id = i;
					return buffer;
				}
			}

			mBuffers.push_back(XAudioBuffer());
			id = mBuffers.size();
			return mBuffers.back();
		}
		XAudioSource* XAudioBackend::GetSourcePtr(const Uint32 audio_source)
		{
			assert(audio_source != 0);
			return &mSources[audio_source - 1];
		}
		//XAudioBuffer* XAudioBackend::GetBufferPtr(const Uint32 audio_source)
		//{
		//	auto source = GetSourcePtr(audio_source);
		//	if (source)
		//		return GetBufferPtr(source.);
		//	else
		//		return nullptr;
		//}
		//XAudioBuffer* XAudioBackend::GetBufferPtr(const Uint32 clip)
		//{
		//	auto id = clip->GetBufferID();
		//	return &mBuffers[id - 1];
		//}
	}
}
