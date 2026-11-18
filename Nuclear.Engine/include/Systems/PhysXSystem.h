#pragma once
#include <NE_Common.h>
#include <ECS/ECS.h>
#include "PhysX\PhysXModule.h"
#include "PhysX\RaycastHit.h"
#include <Components\RigidBodyComponent.h>
#include <Components\ColliderComponent.h>

#include <functional>
#include <memory>
namespace Nuclear::PhysX { class ContactEvents; }
namespace physx { class PxScene; };

namespace Nuclear
{
	namespace Systems
	{

		struct PhysXSystemDesc
		{
			Math::Vector3 mGravity;
		};

		class NEAPI PhysXSystem : public ECS::System<PhysXSystem>
		{
		public:
			PhysXSystem(const PhysXSystemDesc& sceneDesc);
			~PhysXSystem();

			void SetColliderForRigidBody(ECS::Entity entity);

			//Must Be called before update!
			physx::PxScene* GetPhysXScene();
			void SetPhysXScene(physx::PxScene* scene);

			/// Releases actors and shape references belonging to an entity before EnTT removes its components.
			static void ReleaseActor(entt::registry& registry, entt::entity entity);
			/// Called after simulation with both entity IDs, overlap mode and entry/exit state.
			std::function<void(Uint32, Uint32, bool, bool)> OnContact;
			/// Sets a dynamic actor velocity in world units per second. Kinematic or missing actors return false.
			static bool SetVelocity(entt::entity entity, const Math::Vector3& velocity);
			void BeginSimulation(ECS::TimeDelta dt);

			void Bake();

			void AddunAssignedActors();

			bool AddActor(ECS::Entity entity);

			/*	const PxVec3& origin;
	const PxVec3& unitDir;
		const PxReal 	distance,
		PxRaycastCallback& hitCall,
		PxHitFlags 	hitFlags = PxHitFlags(PxHitFlag::eDEFAULT),
		const PxQueryFilterData& filterData = PxQueryFilterData(),
		PxQueryFilterCallback* filterCall = NULL,
		const PxQueryCache* cache = NULL
		*/

			bool Raycast(const Math::Vector3& Origin, const Math::Vector3& UnitDir, const Float32& distance,
				PhysX::RaycastHit& hit
			);

			void Update(ECS::TimeDelta dt) override;
		private:
			std::unique_ptr<PhysX::ContactEvents> mEvents; ///< Buffers callbacks until simulation completes.
			physx::PxScene* mPhysXScene;
		};

	}
}