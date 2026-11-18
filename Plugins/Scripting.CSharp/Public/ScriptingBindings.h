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
			/// Returns a versioned entity ID for the first exact name match, or the EnTT null value.
			Uint32 EntityFind(Nuclear::Managed::String name);
			/// Reads/writes a live entity's local position through three contiguous floats.
			Uint32 EntityPosition(Uint32 id, float* xyz, Uint32 write);
			/// Requests removal after the current managed update pass.
			Uint32 EntityDestroy(Uint32 id);
			/// Sets the gameplay HUD label in the active viewport.
            void SetHudText(Nuclear::Managed::String message);
            /// Replaces or closes the runtime screen. Returns its viewport identity, or zero when closed.
            Uint64 OpenScreen(Nuclear::Managed::String title, Uint32 visible);
            /// Adds a labeled button to the current screen; its command is delivered on the next script update.
            Uint32 ScreenButton(Uint64 screen, Nuclear::Managed::String caption, Uint32 command);
            /// Consumes the oldest clicked command for this screen, or zero when none is pending.
            Uint32 PollScreen(Uint64 screen);
            /// Returns the inspector-authored JSON object for this live script.
            Nuclear::Managed::String ScriptFields(Uint32 id);
            /// Returns the hit entity, or the null entity on a miss or invalid parameters.
            Uint32 Raycast(const float* origin, const float* direction, float distance, float* hit);
            /// Sets a dynamic body's velocity in world units per second.
            Uint32 PlayClip(Uint32 id, int clip, float seconds); ///< Selects an imported skeletal clip and transition duration.
            Uint32 SetVelocity(Uint32 id, const float* velocity);
            /// Controls a ready source. Operations are play, pause and stop.
            Uint32 AudioControl(Uint32 id, Uint32 operation);
            /// Returns an owned name for the current level or game.
            Nuclear::Managed::String GameName(Uint32 level);
            /// Requests a transition at the end of this frame.
            Uint32 SetPaused(Uint32 owner, Uint32 value); ///< Pauses or resumes this owner\'s game simulation.
            Uint32 SpawnPrefab(Nuclear::Managed::String name, const float* position); ///< Reserves a live instance.
            Uint32 FindPersistent(Nuclear::Managed::String identity); ///< Finds an entity using its stable scene identity.
            Uint32 LoadLevel(Nuclear::Managed::String name);
            /// Polls a zero-based controller slot into the native-compatible blittable state.
			Uint32 ReadGamepad(Uint32 slot, Platform::Input::GamepadState* state);
			Uint32 Platform_Input_IsKeyPressed(Platform::Input::KeyCode key);
			void Components_LightComponent_GetColor(Uint32 id, Graphics::Color* outcolor);
			void Components_LightComponent_SetColor(Uint32 id, Graphics::Color* incolor);
			float Components_LightComponent_GetIntensity(Uint32 id);
			void Components_LightComponent_SetIntensity(Uint32 id, float intensity);
		}
	}
}
