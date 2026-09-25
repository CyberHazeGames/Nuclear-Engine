#pragma once
#include <NE_Common.h>
#include <Platform/Input.h>
#include <Graphics/Color.h>

namespace Nuclear::Managed { class String; class ReflectionType; }

namespace Nuclear
{
	namespace Scripting
	{
		namespace Bindings
		{
			void Utilities_Logger_Trace(Nuclear::Managed::String message);
			void Utilities_Logger_Info(Nuclear::Managed::String message);
			void Utilities_Logger_Warn(Nuclear::Managed::String message);
			void Utilities_Logger_Error(Nuclear::Managed::String message);
			void Utilities_Logger_FatalError(Nuclear::Managed::String message);
			Uint32 ECS_Entity_AddComponent(Uint32 id, Nuclear::Managed::ReflectionType type);
			Uint32 ECS_Entity_HasComponent(Uint32 id, Nuclear::Managed::ReflectionType type);
			Nuclear::Managed::String Components_EntityInfoComponent_GetName(Uint32 id);
			void Components_EntityInfoComponent_SetName(Uint32 id, Nuclear::Managed::String name);
			Uint32 Platform_Input_IsKeyPressed(Platform::Input::KeyCode key);
			void Components_LightComponent_GetColor(Uint32 id, Graphics::Color* outcolor);
			void Components_LightComponent_SetColor(Uint32 id, Graphics::Color* incolor);
			float Components_LightComponent_GetIntensity(Uint32 id);
			void Components_LightComponent_SetIntensity(Uint32 id, float intensity);
		}
	}
}
