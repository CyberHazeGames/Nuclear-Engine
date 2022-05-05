#pragma once
#include <Core\NE_Common.h>
#include <Engine/ECS/EntityFactory.h>
#include <Engine/ECS/System.h>
#include <Core\Path.h>

namespace Nuclear
{
	namespace Components { class CameraComponent; }
	namespace ECS
	{

		class NEAPI Scene {
		public:
			Scene();
			~Scene();

			//Creates a new entity and assign a transform component to it automatically
			Entity CreateEntity();
			Entity CreateEntity(const std::string& name, const ECS::Transform& transform = ECS::Transform());

			std::string GetName();

			void Update(ECS::TimeDelta dt);

			SystemManager& GetSystemManager();
			entt::registry& GetRegistry();
			EntityFactory& GetFactory();

			void Save();
		private:
			std::string mName;
			EntityFactory Factory;
			entt::registry Registry;
			SystemManager Systems;

		};
	}
}