#include "ManagedRuntime.h"
#include "../Public/ScriptingRegistry.h"
#include "../Public/ScriptingAssembly.h"
#include <Components/EntityInfoComponent.h>
#include <Components/LightComponent.h>
#include <Nuclear/Managed/Assembly.hpp>

namespace Nuclear
{
	namespace Scripting
	{
		template<typename T>
		void RegisterComponent(ScriptingRegistry& registry, ScriptingAssembly* assembly, const char* name)
		{
			auto& type = assembly->GetAssembly()->GetLocalType(name);
			if (!type)
				return;
			registry.mAddComponentFuncs[type.GetTypeId()] = [](ECS::Entity& entity) {
				if (!entity.HasComponent<T>())
					entity.AddComponent<T>();
			};
			registry.mHasComponentFuncs[type.GetTypeId()] = [](ECS::Entity& entity) {
				return entity.HasComponent<T>();
			};
		}
		void ScriptingRegistry::RegisterEngineComponents(ScriptingAssembly* coreassembly)
		{
			Clear();
			// Register only components with managed wrappers.
			RegisterComponent<Components::EntityInfoComponent>(*this, coreassembly, "Nuclear.Components.EntityInfoComponent");
			RegisterComponent<Components::LightComponent>(*this, coreassembly, "Nuclear.Components.LightComponent");
		}
		void ScriptingRegistry::Clear()
		{
			mAddComponentFuncs.clear();
			mHasComponentFuncs.clear();
		}
	}
}
