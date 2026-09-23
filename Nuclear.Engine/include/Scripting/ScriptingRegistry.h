#pragma once
#include <NE_Common.h>
#include <ECS/Entity.h>
#include <functional>
#include <unordered_map>

namespace Nuclear
{
	namespace Scripting
	{
		class ScriptingAssembly;
		class NEAPI ScriptingRegistry
		{
		public:
			void RegisterEngineComponents(ScriptingAssembly* coreassembly);
			void Clear();
			std::unordered_map<Int32, std::function<bool(ECS::Entity&)>> mHasComponentFuncs;
			std::unordered_map<Int32, std::function<void(ECS::Entity&)>> mAddComponentFuncs;
		};
	}
}
