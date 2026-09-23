#include "ManagedRuntime.h"
#include <Scripting/ScriptingBindings.h>
#include <Scripting/ScriptingModule.h>
#include <Core/Scene.h>
#include <Utilities/Logger.h>
#include <Components/LightComponent.h>
#include <Components/EntityInfoComponent.h>

namespace Nuclear
{
	namespace Scripting
	{
		namespace Bindings
		{
			void Utilities_Logger_Trace(Nuclear::Managed::String message) { CLIENT_TRACE("{0}", std::string(message)); }
			void Utilities_Logger_Info(Nuclear::Managed::String message) { CLIENT_INFO("{0}", std::string(message)); }
			void Utilities_Logger_Warn(Nuclear::Managed::String message) { CLIENT_WARN("{0}", std::string(message)); }
			void Utilities_Logger_Error(Nuclear::Managed::String message) { CLIENT_ERROR("{0}", std::string(message)); }
			void Utilities_Logger_FatalError(Nuclear::Managed::String message) { CLIENT_FATAL("{0}", std::string(message)); }

			Uint32 ECS_Entity_AddComponent(Uint32 id, Nuclear::Managed::ReflectionType type)
			{
				auto& registry = Core::Scene::Get().GetRegistry();
				auto& functions = ScriptingModule::Get().GetRegistry().mAddComponentFuncs;
				auto function = functions.find(type.m_TypeID);
				if (!registry.valid(static_cast<entt::entity>(id)) || function == functions.end())
					return false;
				ECS::Entity entity(registry, id);
				function->second(entity);
				return true;
			}
			Uint32 ECS_Entity_HasComponent(Uint32 id, Nuclear::Managed::ReflectionType type)
			{
				auto& registry = Core::Scene::Get().GetRegistry();
				auto& functions = ScriptingModule::Get().GetRegistry().mHasComponentFuncs;
				auto function = functions.find(type.m_TypeID);
				if (!registry.valid(static_cast<entt::entity>(id)) || function == functions.end())
					return false;
				ECS::Entity entity(registry, id);
				return function->second(entity);
			}
			Nuclear::Managed::String Components_EntityInfoComponent_GetName(Uint32 id)
			{
				auto& registry = Core::Scene::Get().GetRegistry();
				auto entity = static_cast<entt::entity>(id);
				auto info = registry.valid(entity) ? registry.try_get<Components::EntityInfoComponent>(entity) : nullptr;
				return Nuclear::Managed::String::New(info ? info->mName : "");
			}
			void Components_EntityInfoComponent_SetName(Uint32 id, Nuclear::Managed::String name)
			{
				auto& registry = Core::Scene::Get().GetRegistry();
				auto entity = static_cast<entt::entity>(id);
				auto info = registry.valid(entity) ? registry.try_get<Components::EntityInfoComponent>(entity) : nullptr;
				if (info)
					info->mName = std::string(name);
			}
			Uint32 Platform_Input_IsKeyPressed(Platform::Input::KeyCode key)
			{
				return Platform::Input::Get().IsKeyPressed(key);
			}
			static Components::LightComponent* GetLight(Uint32 id)
			{
				auto& registry = Core::Scene::Get().GetRegistry();
				auto entity = static_cast<entt::entity>(id);
				return registry.valid(entity) ? registry.try_get<Components::LightComponent>(entity) : nullptr;
			}
			void Components_LightComponent_GetColor(Uint32 id, Graphics::Color* outcolor)
			{
				auto light = GetLight(id);
				if (outcolor)
					*outcolor = light ? light->GetColor() : Graphics::Color(0.0f);
			}
			void Components_LightComponent_SetColor(Uint32 id, Graphics::Color* incolor)
			{
				if (auto light = GetLight(id); light && incolor)
					light->SetColor(*incolor);
			}
			float Components_LightComponent_GetIntensity(Uint32 id)
			{
				auto light = GetLight(id);
				return light ? light->GetIntensity() : 0.0f;
			}
			void Components_LightComponent_SetIntensity(Uint32 id, float intensity)
			{
				if (auto light = GetLight(id))
					light->SetIntensity(intensity);
			}
		}
	}
}
