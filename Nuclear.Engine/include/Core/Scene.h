#pragma once
#include <NE_Common.h>
#include <ECS/System.h>
#include <Core\Path.h>
#include <Assets/IAsset.h>
#include <ECS\Entity.h>
#include <ECS\Transform.h>
#include <Assets/Scene.h>

namespace Nuclear
{
	namespace Components { class CameraComponent; }
	namespace Assets { class Material; }

	namespace Core
	{
		class NEAPI Scene
		{
		public:
			static Scene& Get();

			Scene(const Scene&) = delete;
			Scene& operator= (const Scene) = delete;

			//Creates a new entity and assign a transform component to it automatically
			ECS::Entity CreateEntity();
			ECS::Entity CreateEntity(const std::string& name, const ECS::Transform& transform = ECS::Transform());

			ECS::Entity CreateBox(Assets::Material* material, const ECS::Transform& transform = ECS::Transform(), bool addcollider = true);
			ECS::Entity CreateSphere(Assets::Material* material, const ECS::Transform& transform = ECS::Transform(), bool addcollider = true);
			ECS::Entity CreatePlane(Assets::Material* material, const ECS::Transform& transform = ECS::Transform(), bool addcollider = true);

			/// Calculates parent-before-child world transforms using persistent entity IDs.
            void UpdateTransforms();
            /// Changes parent after checking existence and ancestry. An empty parent detaches the child.
            bool SetParent(entt::entity child, const std::string& parent);
            void Clear();

			void SetMainCamera(const ECS::Entity& entity);
			void SetMainCamera(Components::CameraComponent* camera);
			Components::CameraComponent* GetMainCamera();

			ECS::SystemManager& GetSystemManager();
			entt::registry& GetRegistry();

			void Update(ECS::TimeDelta dt);

			//Serialization
			bool SaveScene(Assets::Scene* scene);

			bool LoadScene(Assets::Scene* scene);

			std::string GetName() const;
			Assets::Scene* GetSceneAsset();
		protected:
			entt::registry mRegistry;
			ECS::SystemManager mSystems;
			Components::CameraComponent* pMainCamera;

			std::string mCurrentSceneName;
			Assets::Scene* pSceneAsset;
		private:
			Scene();
		};
	}
}