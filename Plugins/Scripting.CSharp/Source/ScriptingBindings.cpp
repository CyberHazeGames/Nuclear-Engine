#include "ManagedRuntime.h"
#include "../Public/ScriptingBindings.h"
#include "../Public/CSharpScriptingBackend.h"
#include <Scripting/ScriptingModule.h>
#include <Core/Scene.h>
#include <Core/Engine.h>
#include <Core/Client.h>
#include <Systems/PhysXSystem.h>
#include <Components/AudioSourceComponent.h>
#include <Components/MeshComponent.h>
#include <Components/ScriptComponent.h>
#include <Systems/ScriptingSystem.h>
#include <Assets/Font.h>
#include <UI/UIModule.h>
#include <UI/TextBlock.h>
#include <UI/Button.h>
#include <UI/VerticalBox.h>
#include <deque>
#include <cmath>
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
				auto* backend = static_cast<CSharp::CSharpScriptingBackend*>(ScriptingModule::Get().FindBackend("CSharp"));
				if (!backend) return false;
				auto& functions = backend->GetRegistry().mAddComponentFuncs;
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
				auto* backend = static_cast<CSharp::CSharpScriptingBackend*>(ScriptingModule::Get().FindBackend("CSharp"));
				if (!backend) return false;
				auto& functions = backend->GetRegistry().mHasComponentFuncs;
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
			Uint32 EntityFind(Nuclear::Managed::String name)
			{
				std::string target(name);
				auto view = Core::Scene::Get().GetRegistry().view<Components::EntityInfoComponent>();
				for (auto entity : view)
					if (view.get<Components::EntityInfoComponent>(entity).mName == target)
						return static_cast<Uint32>(entity);
				return static_cast<Uint32>(static_cast<entt::entity>(entt::null));
			}
			Uint32 EntityPosition(Uint32 id, float* xyz, Uint32 write)
			{
				auto& reg = Core::Scene::Get().GetRegistry();
				auto entity = static_cast<entt::entity>(id);
				auto* info = reg.valid(entity) ? reg.try_get<Components::EntityInfoComponent>(entity) : nullptr;
				if (!info || !xyz) return 0;
				if (write)
				{
					if (!std::isfinite(xyz[0]) || !std::isfinite(xyz[1]) || !std::isfinite(xyz[2])) return 0;
					info->mTransform.SetPosition({xyz[0], xyz[1], xyz[2]});
				}
				else
				{
					const auto p = info->mTransform.GetLocalPosition();
					xyz[0] = p.x; xyz[1] = p.y; xyz[2] = p.z;
				}
				return 1;
			}
			Uint32 EntityDestroy(Uint32 id) { return Systems::ScriptingSystem::RequestDestroy(id); }
			void SetHudText(Nuclear::Managed::String message)
			{
				auto& module = UI::UIModule::Get();
				if (!module.IsInitialized()) return;
				auto& context = module.GetContext();
				static UI::WidgetId labelId = 0;
				auto* label = static_cast<UI::TextBlock*>(context.Find(labelId));
				if (!label)
				{
					auto widget = std::make_unique<UI::TextBlock>();
					label = widget.get();
					labelId = label->GetId();
					label->SetPadding({18, 14, 18, 14});
					context.AddToViewport(std::move(widget), 1);
				}
				label->SetText(std::string(message));
			}
            namespace
            {
                UI::WidgetId currentScreen = 0;
                std::deque<Uint32> commands;
            }
            Uint64 OpenScreen(Nuclear::Managed::String title, Uint32 visible)
            {
                auto& module = UI::UIModule::Get();
                if (!module.IsInitialized()) return 0;
                auto& context = module.GetContext();
                if (context.Find(currentScreen)) context.RemoveFromViewport(currentScreen);
                currentScreen = 0;
                commands.clear();
                context.SetInputMode(visible ? UI::InputMode::GameAndUI : UI::InputMode::GameOnly);
                if (visible)
                {
                    auto panel = std::make_unique<UI::VerticalBox>();
                    panel->SetPadding({24, 48, 24, 24});
                    panel->SetSpacing(8);
                    panel->Add<UI::TextBlock>(std::string(title));
                    currentScreen = panel->GetId();
                    context.AddToViewport(std::move(panel), 10);
                }
                return currentScreen;
            }
            Uint32 ScreenButton(Uint64 screen, Nuclear::Managed::String caption, Uint32 command)
            {
                if (!screen || screen != currentScreen || !command) return 0;
                auto& context = UI::UIModule::Get().GetContext();
                auto* panel = dynamic_cast<UI::VerticalBox*>(context.Find(screen));
                if (!panel) return 0;
                auto& button = panel->Add<UI::Button>();
                button.SetContent(std::make_unique<UI::TextBlock>(std::string(caption)));
                button.OnClicked = [command]() { commands.push_back(command); };
                if (command == 1) context.SetFocus(button.GetId());
                return 1;
            }
            Uint32 PollScreen(Uint64 screen)
            {
                if (screen != currentScreen || commands.empty()) return 0;
                auto command = commands.front();
                commands.pop_front();
                return command;
            }
            Nuclear::Managed::String ScriptFields(Uint32 id)
            {
                auto& reg = Core::Scene::Get().GetRegistry();
                auto* script = reg.try_get<Components::ScriptComponent>(static_cast<entt::entity>(id));
                return Nuclear::Managed::String::New(script ? script->Fields : "");
            }
            Uint32 PlayClip(Uint32 id, int clip, float seconds)
            {
                auto* mesh = Core::Scene::Get().GetRegistry().try_get<Components::MeshComponent>(static_cast<entt::entity>(id));
                return mesh && mesh->PlayClip(clip, seconds);
            }
            Uint32 SetVelocity(Uint32 id, const float* v)
            {
                return v && Systems::PhysXSystem::SetVelocity(static_cast<entt::entity>(id), {v[0], v[1], v[2]});
            }
            Uint32 AudioControl(Uint32 id, Uint32 operation)
            {
                auto& reg = Core::Scene::Get().GetRegistry();
                auto* source = reg.try_get<Components::AudioSourceComponent>(static_cast<entt::entity>(id));
                if (!source || !source->GetSourceID()) return false;
                switch (operation)
                {
                    case 0: source->Play(); break;
                    case 1: source->Pause(); break;
                    case 2: source->Stop(); break;
                    default: return false;
                }
                return true;
            }
            Uint32 Raycast(const float* origin, const float* direction, float distance, float* hit)
            {
                const Uint32 none = static_cast<Uint32>(static_cast<entt::entity>(entt::null));
                auto* client = Core::Engine::Get().GetClient();
                auto* world = client ? client->GetPhysicsWorld() : nullptr;
                if (!world || !origin || !direction || !hit || !std::isfinite(distance) || distance <= 0) return none;
                Math::Vector3 o(origin[0], origin[1], origin[2]), d(direction[0], direction[1], direction[2]);
                float length = Math::length(d);
                if (!std::isfinite(length) || length < 0.000001f || !std::isfinite(o.x) || !std::isfinite(o.y) || !std::isfinite(o.z)) return none;
                PhysX::RaycastHit result;
                if (!world->Raycast(o, d / length, distance, result)) return none;
                for (int i = 0; i < 3; ++i) { hit[i] = result.position[i]; hit[i + 3] = result.normal[i]; }
                hit[6] = result.distance;
                return static_cast<Uint32>(result.HitEntity.GetEntityID());
            }
            Nuclear::Managed::String GameName(Uint32 level)
            {
                const auto* game = Core::Engine::Get().GetClient();
                return Nuclear::Managed::String::New(game ? (level ? game->GetLevelName() : game->GetGameName()) : "");
            }
            Uint32 SetPaused(Uint32 owner, Uint32 value)
            {
                auto* client = Core::Engine::Get().GetClient();
                return client && client->SetGamePaused(owner, value != 0);
            }
            Uint32 SpawnPrefab(Nuclear::Managed::String name, const float* p)
            {
                auto* client = Core::Engine::Get().GetClient();
                return client && p ? client->SpawnPrefab(std::string(name), p[0], p[1], p[2]) : UINT32_MAX;
            }
            Uint32 FindPersistent(Nuclear::Managed::String identity)
            {
                const std::string key(identity);
                auto view = Core::Scene::Get().GetRegistry().view<Components::EntityInfoComponent>();
                for (auto entity : view)
                    if (view.get<Components::EntityInfoComponent>(entity).PersistentId == key) return static_cast<Uint32>(entity);
                return UINT32_MAX;
            }
            Uint32 LoadLevel(Nuclear::Managed::String name)
            {
                auto* game = Core::Engine::Get().GetClient();
                return game && game->RequestLevel(std::string(name));
            }
            Uint32 ReadGamepad(Uint32 slot, Platform::Input::GamepadState* state)
			{
				return state && Platform::Input::ReadGamepad(slot, *state);
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
