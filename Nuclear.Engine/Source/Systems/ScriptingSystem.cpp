#include "Systems\ScriptingSystem.h"
#include "Scripting\ScriptingModule.h"
#include "Components\ScriptComponent.h"
#include "Core\Scene.h"
#include <Utilities/Logger.h>

namespace Nuclear
{
	namespace Systems
	{
		ScriptingSystem::ScriptingSystem()
		{
		}
		ScriptingSystem::~ScriptingSystem()
		{
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
			// connects a free function
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
		}

	}
}
