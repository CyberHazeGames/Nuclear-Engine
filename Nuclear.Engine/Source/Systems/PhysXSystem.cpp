#include <Systems\PhysXSystem.h>
#include <Components\ColliderComponent.h>
#include <Components\RigidBodyComponent.h>
#include <Components\EntityInfoComponent.h>
#include "..\PhysX\PhysXTypes.h"
#include <PhysX/include/PxPhysicsAPI.h>
#include <Core\Scene.h>
#include "../PhysX/ContactEvents.h"

namespace Nuclear
{
	namespace Systems
	{
		PhysXSystem::PhysXSystem(const PhysXSystemDesc &sceneDesc)
		{
			
			PhysX::PxSceneDesc SceneDesc(PhysX::PhysXModule::Get().GetPhysics()->getTolerancesScale());
			SceneDesc.gravity = PhysX::To(sceneDesc.mGravity);
			SceneDesc.cpuDispatcher = PhysX::PhysXModule::Get().GetCPUDispatcher();
			mEvents = std::make_unique<PhysX::ContactEvents>();
            SceneDesc.simulationEventCallback = mEvents.get();
            SceneDesc.filterShader = [](physx::PxFilterObjectAttributes a, physx::PxFilterData ad,
                physx::PxFilterObjectAttributes b, physx::PxFilterData bd, physx::PxPairFlags& flags,
                const void* data, physx::PxU32 bytes) -> physx::PxFilterFlags {
                const auto result = physx::PxDefaultSimulationFilterShader(a, ad, b, bd, flags, data, bytes);
                flags |= physx::PxPairFlag::eNOTIFY_TOUCH_FOUND | physx::PxPairFlag::eNOTIFY_TOUCH_LOST;
                return result;
            };
	
			mPhysXScene = PhysX::PhysXModule::Get().GetPhysics()->createScene(SceneDesc);
		}
		PhysXSystem::~PhysXSystem()
		{
			mPhysXScene->release();
		}

		void PhysXSystem::SetColliderForRigidBody(ECS::Entity entity)
		{
			//Check if entity is valid
			auto RigidComponent = entity.TryToGetComponent<Components::RigidBodyComponent>();
			auto ColliderComponent = entity.TryToGetComponent<Components::ColliderComponent>();

			if (RigidComponent && ColliderComponent)
			{
				RigidComponent->GetActor().mPtr->attachShape(*ColliderComponent->GetShape().mPtr);
			}
		}
		
		physx::PxScene* PhysXSystem::GetPhysXScene()
		{
			return mPhysXScene;
		}

		void PhysXSystem::SetPhysXScene(physx::PxScene* scene)
		{
			mPhysXScene = scene;
		}

		void PhysXSystem::ReleaseActor(entt::registry& registry, entt::entity entity)
		{
			if (auto* body = registry.try_get<Components::RigidBodyComponent>(entity))
				if (auto actor = std::exchange(body->GetActor().mPtr, nullptr)) actor->release();
			if (auto* collider = registry.try_get<Components::ColliderComponent>(entity))
			{
				if (auto actor = std::exchange(collider->GetActor().mPtr, nullptr)) actor->release();
				if (auto shape = std::exchange(collider->GetShape().mPtr, nullptr)) shape->release();
			}
		}

        bool PhysXSystem::SetVelocity(entt::entity entity, const Math::Vector3& velocity)
        {
            auto& registry = Core::Scene::Get().GetRegistry();
            auto* body = registry.valid(entity) ? registry.try_get<Components::RigidBodyComponent>(entity) : nullptr;
            if (!body || !body->GetActor().mPtr || body->isKinematic || !PhysX::To(velocity).isFinite()) return false;
            body->GetActor().mPtr->setLinearVelocity(PhysX::To(velocity));
            return true;
        }

        void PhysXSystem::BeginSimulation(ECS::TimeDelta dt)
		{
			mPhysXScene->simulate(dt);
		}
		void PhysXSystem::Bake()
		{
			auto eView = Core::Scene::Get().GetRegistry().view<Components::ColliderComponent>();
			for (auto entity : eView)
			{
				auto& Obj = eView.get<Components::ColliderComponent>(entity);

				auto RigidComponent = Core::Scene::Get().GetRegistry().try_get<Components::RigidBodyComponent>(entity);
				if (RigidComponent)
				{		
					RigidComponent->GetActor().mPtr->attachShape(*Obj.GetShape().mPtr);
					mPhysXScene->addActor(*RigidComponent->GetActor().mPtr);
				}
				else {
					mPhysXScene->addActor(*Obj.GetActor().mPtr);
				}
			}
		}

		void PhysXSystem::AddunAssignedActors()
		{
			auto eView = Core::Scene::Get().GetRegistry().view<Components::ColliderComponent>();
			for (auto entity : eView)
			{
				auto& Obj = eView.get<Components::ColliderComponent>(entity);
				if (!Obj.isValid())
				{
					auto RigidComponent = Core::Scene::Get().GetRegistry().try_get<Components::RigidBodyComponent>(entity);
					if (RigidComponent)
					{
						RigidComponent->GetActor().mPtr->attachShape(*Obj.GetShape().mPtr);
						mPhysXScene->addActor(*RigidComponent->GetActor().mPtr);
					}
					else {
						mPhysXScene->addActor(*Obj.GetActor().mPtr);
					}

					Obj.SetValid(true);
				}
			}
		}

		bool PhysXSystem::AddActor(ECS::Entity entity)
		{
			auto RigidComponent = entity.TryToGetComponent<Components::RigidBodyComponent>();
			auto ColliderComponent = entity.TryToGetComponent<Components::ColliderComponent>();

			if (RigidComponent && ColliderComponent)
			{
				mPhysXScene->addActor(*RigidComponent->GetActor().mPtr);
				RigidComponent->GetActor().mPtr->userData = (void*)entity.GetEntityID();
				ColliderComponent->GetActor().mPtr->userData = (void*)entity.GetEntityID();
				return true;
			}

			if (ColliderComponent)
			{
				mPhysXScene->addActor(*ColliderComponent->GetActor().mPtr);
				ColliderComponent->GetActor().mPtr->userData = (void*)entity.GetEntityID();
				return true;
			}

			return false;
		}

		//bool PhysXSystem::Raycast()
		//{
		//	PxReal
		///*	const PxVec3& origin;
		//	const PxVec3& unitDir;
		//		const PxReal 	distance,
		//		PxRaycastCallback& hitCall,
		//		PxHitFlags 	hitFlags = PxHitFlags(PxHitFlag::eDEFAULT),
		//		const PxQueryFilterData& filterData = PxQueryFilterData(),
		//		PxQueryFilterCallback* filterCall = NULL,
		//		const PxQueryCache* cache = NULL
		//		*/
		//	return mPhysXScene->raycast();
		//}

		bool PhysXSystem::Raycast(const Math::Vector3& Origin, const Math::Vector3& UnitDir, const Float32& distance, PhysX::RaycastHit& outhit)
		{
			physx::PxRaycastBuffer hit;

			if (mPhysXScene->raycast(PhysX::To(Origin), PhysX::To(UnitDir), distance, hit))
			{
				outhit.position = PhysX::From(hit.block.position);
				outhit.normal = PhysX::From(hit.block.normal);
				outhit.distance = hit.block.distance;
				outhit.HitEntity = ECS::Entity(Core::Scene::Get().GetRegistry(), static_cast<entt::entity>(reinterpret_cast<uintptr_t>(hit.block.actor->userData)));
					
				outhit.valid = true;
				return true;
			}

			return false;
		}

		void PhysXSystem::Update(ECS::TimeDelta dt)
		{
			auto& scene = Core::Scene::Get();
            scene.UpdateTransforms();
            auto bodies = scene.GetRegistry().view<Components::RigidBodyComponent, Components::EntityInfoComponent>();
            for (auto entity : bodies)
            {
                auto& body = bodies.get<Components::RigidBodyComponent>(entity);
                if (!body.GetActor().mPtr) continue;
                if (body.GetInternalisKinematic() != body.isKinematic) body.SetisKinematic(body.isKinematic);
                if (body.isKinematic)
                    body.GetActor().mPtr->setKinematicTarget(PhysX::To(bodies.get<Components::EntityInfoComponent>(entity).mTransform));
            }
            mPhysXScene->simulate(dt);
            mPhysXScene->fetchResults(true);
            for (auto entity : bodies)
            {
                auto& body = bodies.get<Components::RigidBodyComponent>(entity);
                if (body.isKinematic || !body.GetActor().mPtr) continue;
                auto& transform = bodies.get<Components::EntityInfoComponent>(entity).mTransform;
                auto pose = body.GetActor().mPtr->getGlobalPose();
                transform.SetPosition(transform.ToLocalPosition(PhysX::From(pose.p)));
                transform.SetRotation(transform.ToLocalRotation(PhysX::From(pose.q)));
            }
            for (const auto& [first, second, trigger, entering] : mEvents->Pending)
                if (OnContact) OnContact(first, second, trigger, entering);
            mEvents->Pending.clear();
		}
	}
}