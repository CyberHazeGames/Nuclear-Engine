#include "Systems\ScriptingSystem.h"
#include "Scripting\ScriptingModule.h"
#include "Components\ScriptComponent.h"
#include "Core\Scene.h"
#include <Utilities/Logger.h>
#include <Components/CameraComponent.h>
#include <Components/AudioSourceComponent.h>
#include <Systems/PhysXSystem.h>
#include <Components/LightComponent.h>
#include <algorithm>
#include <utility>

namespace Nuclear
{
	namespace Systems
	{
		void ScriptInitialization(entt::registry&, entt::entity);
		ScriptingSystem* ScriptingSystem::pActive = nullptr;

		ScriptingSystem::ScriptingSystem()
		{
		}
		ScriptingSystem::~ScriptingSystem()
		{
			if (pActive == this)
			{
				Core::Scene::Get().GetRegistry().on_construct<Components::ScriptComponent>().disconnect<&ScriptInitialization>();
				pActive = nullptr;
			}
		}
		bool ScriptingSystem::RequestDestroy(Uint32 id)
		{
			if (!pActive) return false;
			auto& scene = Core::Scene::Get();
			auto& reg = scene.GetRegistry();
			auto entity = static_cast<entt::entity>(id);
			if (!reg.valid(entity)) return false;
			if (auto* camera = reg.try_get<Components::CameraComponent>(entity); camera && camera == scene.GetMainCamera())
				return false;
			auto& pending = pActive->mPendingDestroy;
			if (std::find(pending.begin(), pending.end(), id) != pending.end()) return false;
			pending.push_back(id);
			return true;
		}
		void ScriptInitialization(entt::registry& reg, entt::entity entity)
		{
			auto& script = reg.get<Components::ScriptComponent>(entity);
			auto scriptasset = script.GetScriptAsset();
			if (scriptasset)
			{
				Uint32 entityID = static_cast<Uint32>(entity);
				script.GetScriptingObject() = scriptasset->mClass.CreateObject(entityID);
				script.GetScriptingObject().CallMethod(scriptasset->mOnStartMethod);
			}


		}

		bool ScriptingSystem::Initialize()
		{
			if (pActive && pActive != this) return false;
			pActive = this;
			Core::Scene::Get().GetRegistry().on_construct<Components::ScriptComponent>().connect<&ScriptInitialization>();

			

			NUCLEAR_INFO("[ScriptingSystem] ScriptingSystem initialized successfully...");
			return true;
		}

		void ScriptingSystem::Load()
		{
			//auto view = Core::Scene::Get().GetRegistry().view<Components::ScriptComponent>();

			//for (auto entity : view)
			//{
			//	auto& script = view.get<Components::ScriptComponent>(entity);

			//	//script.mScriptObject.CallMethod(script.mScriptObject.GetScript()->mLoadMethod);
			//}
		}

		void ScriptingSystem::Update(ECS::TimeDelta dt)
		{
			auto view = Core::Scene::Get().GetRegistry().view<Components::ScriptComponent>();

			for (auto entity : view)
			{
				auto& script = view.get<Components::ScriptComponent>(entity);
				auto pScriptasset = script.GetScriptAsset();
				if (pScriptasset)
				{
					script.GetScriptingObject().CallMethod(pScriptasset->mOnUpdateMethod, dt);
				}

			}
			for (auto id : std::exchange(mPendingDestroy, {}))
			{
				auto& registry = Core::Scene::Get().GetRegistry();
				auto entity = static_cast<entt::entity>(id);
				if (!registry.valid(entity)) continue;
				Systems::PhysXSystem::ReleaseActor(registry, entity);
				if (auto* source = registry.try_get<Components::AudioSourceComponent>(entity)) source->Stop();
				registry.destroy(entity);
			}

		}

	}
}
