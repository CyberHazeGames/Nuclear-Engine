#include "Components\ColliderComponent.h"
#include <PhysX/include/PxPhysicsAPI.h>
#include "..\PhysX\PhysXTypes.h"
#include "PhysX\PhysXModule.h"

namespace Nuclear
{
	namespace Components
	{
		ColliderComponent::ColliderComponent()
		{
		}
		ColliderComponent::ColliderComponent(const ColliderDesc& desc)
		{
			Create(desc);
		}

		ColliderComponent::~ColliderComponent()
		{
		}
		void ColliderComponent::Create(const ColliderDesc& desc)
		{
			mType = desc.mShape;
			physx::PxMaterial* material = PhysX::PhysXModule::Get().GetPhysics()->createMaterial(desc.mStaticFriction, desc.mDynamicFriction, desc.mRestitution);

			if (mType == ColliderShape::Box)
			{
				PhysX::BoxGeometry* Geo = (PhysX::BoxGeometry*)desc.mGeo;

				mShape.mPtr = PhysX::PhysXModule::Get().GetPhysics()->createShape(
					physx::PxBoxGeometry(Geo->mHalfExtents.x, Geo->mHalfExtents.y, Geo->mHalfExtents.z), *material);

				mStaticActor.mPtr = PhysX::PhysXModule::Get().GetPhysics()->createRigidStatic(PhysX::To(Geo->mTransform));
				if (IsTrigger)
				{
					mShape.mPtr->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
					mShape.mPtr->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, true);
				}
				mStaticActor.mPtr->attachShape(*mShape.mPtr);
			}
			else if (mType == ColliderShape::Plane)
			{
				//PhysX::BoxGeometry* Geo = (PhysX::BoxGeometry*)desc.mGeo;
				//PhysX::PlaneGeometry* Geo = (PhysX::PlaneGeometry*)desc.mGeo;
				mStaticActor.mPtr = PxCreatePlane(*PhysX::PhysXModule::Get().GetPhysics(), PhysX::PxPlane(0, 1, 0, 0), *material);
			}
			else if (mType == ColliderShape::Sphere)
			{
				PhysX::SphereGeometry* Geo = (PhysX::SphereGeometry*)desc.mGeo;

				mShape.mPtr = PhysX::PhysXModule::Get().GetPhysics()->createShape(physx::PxSphereGeometry(Geo->radius), *material);
				mStaticActor.mPtr = PhysX::PhysXModule::Get().GetPhysics()->createRigidStatic(PhysX::To(Geo->mTransform));
				if (IsTrigger)
				{
					mShape.mPtr->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
					mShape.mPtr->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, true);
				}
				mStaticActor.mPtr->attachShape(*mShape.mPtr);
			}
			else
			{
				assert(true);
			}

			material->release();
		}
		bool ColliderComponent::isValid()
		{
			return mAddedtoPhysxScene;
		}
		void ColliderComponent::SetValid(bool value)
		{
			mAddedtoPhysxScene = value;
		}
		PhysX::PhysXShape& ColliderComponent::GetShape()
		{
			return	mShape;
		}
		PhysX::RigidStatic& ColliderComponent::GetActor()
		{
			return mStaticActor;
				
		}
	}
}