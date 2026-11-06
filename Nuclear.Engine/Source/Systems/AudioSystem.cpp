#include <Systems/AudioSystem.h>
#include <Components/EntityInfoComponent.h>
#include <Components/AudioSourceComponent.h>
#include <Components/AudioListenerComponent.h>
#include <Audio/AudioModule.h>
#include <Audio/AudioBackend.h>

#include <Core/Scene.h>
#include <Utilities/Logger.h>

namespace Nuclear
{
	namespace Systems
	{
		void AudioSourceInit(entt::registry& reg, entt::entity entity)
		{
			auto& source = reg.get<Components::AudioSourceComponent>(entity);
			auto& transform = reg.get<Components::EntityInfoComponent>(entity).mTransform;

			if (!source.GetAudioClip())
			{
				NUCLEAR_WARN("[AudioSystem] Creating AudioSourceComponent with no audio clip!");
				return;
			}
			const auto state = source.GetAudioClip()->GetState();
			if ((state != Assets::IAsset::State::Loaded && state != Assets::IAsset::State::Created) ||
				source.GetAudioClip()->GetBufferID() == 0)
			{
				NUCLEAR_ERROR("[AudioSystem] Creating AudioSourceComponent with invalid audio clip!");
				return;
			}

			auto* backend = Audio::AudioModule::Get().GetBackend();
			if (!backend || !backend->CreateAudioSource(&source, transform))
			{
				return;
			}
			backend->SetSource_Transform(source.GetSourceID(), transform.GetLocalPosition(), transform.GetLocalRotation());
			backend->SetSource_Velocity(source.GetSourceID(), Math::Vector3(0.0f));
		}

		void AudioListenerInit(entt::registry& reg, entt::entity entity)
		{
			auto& listener = reg.get<Components::AudioListenerComponent>(entity);
			auto& transform = reg.get<Components::EntityInfoComponent>(entity).mTransform;

			Audio::AudioModule::Get().GetBackend()->SetListener_Transform(transform.GetLocalPosition(), transform.GetLocalRotation());
			Audio::AudioModule::Get().GetBackend()->SetListener_Velocity(Math::Vector3(0.0f));
		}

		AudioSystem::AudioSystem()
		{
			Core::Scene::Get().GetRegistry().on_construct<Components::AudioListenerComponent>().connect<&AudioListenerInit>();
			Core::Scene::Get().GetRegistry().on_construct<Components::AudioSourceComponent>().connect<&AudioSourceInit>();
		}
		AudioSystem::~AudioSystem()
		{
		}
		void AudioSystem::Update(ECS::TimeDelta dt)
		{

			auto listener_view = Core::Scene::Get().GetRegistry().view<Components::AudioListenerComponent>();
			for (auto entity : listener_view)
			{
				auto& audio_listener = listener_view.get<Components::AudioListenerComponent>(entity);

				//if(audio_listener.mIsActive)
				auto Einfo = Core::Scene::Get().GetRegistry().try_get<Components::EntityInfoComponent>(entity);

				const Math::Vector3 pos = Einfo->mTransform.GetLocalPosition();

				if (audio_listener.mPrevPos != pos)
				{
					const Math::Vector3 prevVelocity = audio_listener.mVelocity;
					audio_listener.mVelocity = (pos - audio_listener.mPrevPos) / dt;
					audio_listener.mPrevPos = pos;
					if (audio_listener.mVelocity != prevVelocity)
					{
					//	Audio::AudioModule::Get().GetBackend()->SetListener_Velocity(audio_listener.mVelocity);
					}
					Audio::AudioModule::Get().GetBackend()->SetListener_Transform(pos, Einfo->mTransform.GetLocalRotation());

				}
			}


			auto eView = Core::Scene::Get().GetRegistry().view<Components::AudioSourceComponent>();
			for (auto entity : eView)
			{
				auto& audio_source = eView.get<Components::AudioSourceComponent>(entity);				
				auto Einfo = Core::Scene::Get().GetRegistry().try_get<Components::EntityInfoComponent>(entity);

				auto* clip = audio_source.GetAudioClip();
				if (!clip || audio_source.GetSourceID() == 0 || !Einfo)
				{
					continue;
				}
				const auto state = clip->GetState();
				if (state != Assets::IAsset::State::Loaded && state != Assets::IAsset::State::Created)
				{
					continue;
				}
				if (clip->Is3D())
				{
					const Math::Vector3 pos = Einfo->mTransform.GetLocalPosition();

					if (audio_source.mPrevPos != pos)
					{
						const Math::Vector3 prevVelocity = audio_source.mVelocity;
					//	audio_source.mVelocity = (pos - audio_source.mPrevPos) / dt;
						audio_source.mPrevPos = pos;
						if (audio_source.mVelocity != prevVelocity)
						{
						//	Audio::AudioModule::Get().GetBackend()->SetSource_Velocity(audio_source.GetSourceID(), audio_source.mVelocity);
						}
						Audio::AudioModule::Get().GetBackend()->SetSource_Transform(audio_source.GetSourceID(), pos, Einfo->mTransform.GetLocalRotation());

					}
				}
			}

			Audio::AudioModule::Get().GetBackend()->Update();
		}
	}
}
