#include <unordered_set>
#include <Core\Scene.h>
#include <sstream>
#include <Components\Components.h>
#include <Systems\PhysXSystem.h>
#include <Assets/DefaultMeshes.h>
#include "..\PhysX\PhysXTypes.h"
#include <Assets/Material.h>
#include <Core/Engine.h>
#include <Utilities/Logger.h>

#include "Serialization/MathSerialization.h"
#include "Serialization/SceneArchive.h"

#include <Assets/AssetManager.h>
#include <Assets/Shader.h>
#include <Assets/Texture.h>


namespace Nuclear 
{
	namespace Core
	{
		Scene& Scene::Get()
		{
			static Scene instance;

			return instance;
		}
		void CameraInitialization(entt::registry& reg, entt::entity entity)
		{
			auto& camera = reg.get<Components::CameraComponent>(entity);
			auto& eInfo = reg.get<Components::EntityInfoComponent>(entity);

			NUCLEAR_INFO("[Scene] Registered a new Camera to Entity: ", eInfo.mName);
		}
		ECS::Entity Scene::CreateEntity()
		{
			ECS::Entity result(GetRegistry(), GetRegistry().create());
			auto& einfo = result.AddComponent<Components::EntityInfoComponent>();

			return result;
		}
		ECS::Entity Scene::CreateEntity(const std::string& name, const ECS::Transform& transform)
		{
			ECS::Entity result(GetRegistry(), GetRegistry().create());
			auto& einfo = result.AddComponent<Components::EntityInfoComponent>(transform, name);

			return result;
		}

		ECS::Entity Scene::CreateBox(Assets::Material* material, const ECS::Transform& transform, bool addcollider)
		{
			ECS::Entity Result = CreateEntity("Box", transform);

			Result.AddComponent<Components::MeshComponent>(Assets::DefaultMeshes::GetCubeAsset(), material);

			if (addcollider)
			{
				auto System = GetSystemManager().GetSystem<Systems::PhysXSystem>();
				if (System.get() != nullptr && System->GetPhysXScene() != nullptr)
				{
					PhysX::BoxGeometry box(Math::Vector3(0.5f, 0.5f, 0.5f), transform);
					Components::ColliderDesc desc;
					desc.mGeo = &box;
					desc.mShape = Components::ColliderShape::Box;
					Result.AddComponent<Components::ColliderComponent>(desc);
					Result.AddComponent<Components::RigidBodyComponent>(transform);
					System->AddActor(Result);
					System->SetColliderForRigidBody(Result);
					Result.GetComponent<Components::ColliderComponent>().SetValid(true);
				}
			}

			return Result;
		}

		ECS::Entity Scene::CreateSphere(Assets::Material* material, const ECS::Transform& transform, bool addcollider)
		{
			ECS::Entity Result = CreateEntity("Sphere", transform);
			Result.AddComponent<Components::MeshComponent>(Assets::DefaultMeshes::GetSphereAsset(), material);

			if (addcollider)
			{
				auto System = GetSystemManager().GetSystem<Systems::PhysXSystem>();
				if (System.get() != nullptr && System->GetPhysXScene() != nullptr)
				{
					PhysX::SphereGeometry sphere(1.0f, transform);
					Components::ColliderDesc desc;
					desc.mGeo = &sphere;
					desc.mShape = Components::ColliderShape::Sphere;
					Result.AddComponent<Components::ColliderComponent>(desc);
					Result.AddComponent<Components::RigidBodyComponent>(transform);
					System->AddActor(Result);
					System->SetColliderForRigidBody(Result);
					Result.GetComponent<Components::ColliderComponent>().SetValid(true);
				}
			}

			return Result;
		}

		ECS::Entity Scene::CreatePlane(Assets::Material* material, const ECS::Transform& transform, bool addcollider)
		{
			ECS::Entity Result = CreateEntity("Plane", transform);
			Result.AddComponent<Components::MeshComponent>(Assets::DefaultMeshes::GetPlaneAsset(), material);

			if (addcollider)
			{
				auto System = GetSystemManager().GetSystem<Systems::PhysXSystem>();
				if (System.get() != nullptr && System->GetPhysXScene() != nullptr)
				{
					PhysX::PlaneGeometry plane(transform);
					Components::ColliderDesc desc;
					desc.mGeo = &plane;
					desc.mShape = Components::ColliderShape::Plane;
					Result.AddComponent<Components::ColliderComponent>(desc);
					System->AddActor(Result);
				}
			}

			return Result;
		}

		bool Scene::SetParent(entt::entity child, const std::string& parent)
        {
            auto* info = mRegistry.try_get<Components::EntityInfoComponent>(child);
            if (!info) return false;
            std::unordered_map<std::string, std::string> parents;
            auto view = mRegistry.view<Components::EntityInfoComponent>();
            for (auto id : view)
            {
                auto& item = view.get<Components::EntityInfoComponent>(id);
                parents.emplace(item.PersistentId, item.ParentId);
            }
            std::unordered_set<std::string> seen{info->PersistentId};
            for (auto id = parent; !id.empty(); id = parents.at(id))
                if (!parents.contains(id) || !seen.insert(id).second) return false;
            info->ParentId = parent;
            UpdateTransforms();
            return true;
        }

        void Scene::UpdateTransforms()
        {
            auto view = mRegistry.view<Components::EntityInfoComponent>();
            bool hasParents = false;
            for (auto id : view)
                if (!view.get<Components::EntityInfoComponent>(id).ParentId.empty()) { hasParents = true; break; }
            if (!hasParents)
            {
                for (auto id : view) view.get<Components::EntityInfoComponent>(id).mTransform.SetParentMatrix(Math::Matrix4(1.f));
                return;
            }
            std::unordered_map<std::string, Components::EntityInfoComponent*> nodes;
            for (auto id : view)
            {
                auto& item = view.get<Components::EntityInfoComponent>(id);
                nodes.emplace(item.PersistentId, &item);
            }
            std::unordered_set<Components::EntityInfoComponent*> done;
            std::vector<Components::EntityInfoComponent*> chain;
            for (auto id : view)
            {
                auto* item = &view.get<Components::EntityInfoComponent>(id);
                while (item && done.insert(item).second)
                {
                    chain.push_back(item);
                    auto parent = nodes.find(item->ParentId);
                    item = parent == nodes.end() ? nullptr : parent->second;
                }
                while (!chain.empty())
                {
                    auto* node = chain.back();
                    chain.pop_back();
                    const auto parent = nodes.find(node->ParentId);
                    node->mTransform.SetParentMatrix(parent == nodes.end() ? Math::Matrix4(1.f) : parent->second->mTransform.GetWorldMatrix());
                }
            }
        }

        void Scene::Clear()
		{
			mSystems.Clear();
			mRegistry.clear();
		}

		void Scene::SetMainCamera(const ECS::Entity& entity)
		{
			pMainCamera = entity.TryToGetComponent<Components::CameraComponent>();
		}

		void Scene::SetMainCamera(Components::CameraComponent* camera)
		{
			pMainCamera = camera;
		}

		Components::CameraComponent* Scene::GetMainCamera()
		{
			return pMainCamera;
		}

		ECS::SystemManager& Scene::GetSystemManager()
		{
			return mSystems;
		}

		entt::registry& Scene::GetRegistry()
		{
			return mRegistry;
		}

		void Scene::Update(ECS::TimeDelta dt)
		{
			return mSystems.Update_All(dt);
		}

		// some helper types
		using Buffer = std::vector<Uint8>;

		bool Scene::SaveScene(Assets::Scene* scene)
		{
			scene->mBinaryBuffer.clear();

			zpp::bits::out out(scene->mBinaryBuffer);

			//bitsery::Serializer<OutputAdapter> ser{ scene->mBinaryBuffer };
			Serialization::SceneOutputArchive<zpp::bits::out<Buffer>> output(&out);
			entt::snapshot{ mRegistry }.entities(output).component < Components::EntityInfoComponent, Components::LightComponent, Components::MeshComponent >(output);

			//ser.object(Assets::AssetManager::Get().mLibrary);

			return true;
		}
		bool Scene::LoadScene(Assets::Scene* scene)
		{
			pSceneAsset = scene;
			mRegistry.clear();

			if (scene->mBinaryBuffer.size() != 0)
			{
				zpp::bits::in in(scene->mBinaryBuffer);
				Serialization::SceneInputArchive<zpp::bits::in<Buffer>> input(&in);
				entt::snapshot_loader{ mRegistry }.entities(input).component<Components::EntityInfoComponent, Components::LightComponent, Components::MeshComponent>(input);
			}

			return true;
		}

		std::string Scene::GetName() const
		{
			return mCurrentSceneName;
		}

		Assets::Scene* Scene::GetSceneAsset()
		{
			return pSceneAsset;
		}

		Scene::Scene()
		{
			GetRegistry().on_construct<Components::CameraComponent>().connect<&CameraInitialization>();

			mCurrentSceneName = "SCENE_CTOR";
			pMainCamera = nullptr;
		}
	}
}