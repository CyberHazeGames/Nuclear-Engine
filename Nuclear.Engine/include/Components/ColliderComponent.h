#pragma once
#include <NE_Common.h>
#include <Math\Math.h>
#include <PhysX/PhysXModule.h>
#include <PhysX/PhysXMaterial.h>
#include <PhysX/PhysXShape.h>
#include <PhysX/RigidStatic.h>
#include <PhysX/Geometry.h>

namespace Nuclear
{
	namespace Components
	{
		enum class ColliderShape
		{
			Unknown,
			Box,
			Plane,
			Sphere
		};

		struct NEAPI ColliderDesc
		{
			float mStaticFriction = 0.5f;
			float mDynamicFriction = 0.5f;
			float mRestitution = 0.5f;
			ColliderShape mShape = ColliderShape::Unknown;
			PhysX::PhysicsGeometry* mGeo = nullptr;
		};

		class NEAPI ColliderComponent
		{
		public:
			bool IsTrigger = false; ///< Overlap-only shape that reports entry and exit without collision response.
			ColliderComponent();
			ColliderComponent(const ColliderDesc& desc);
			~ColliderComponent();

			void Create(const ColliderDesc& desc);
			bool isValid();
			void SetValid(bool value);

			PhysX::PhysXShape& GetShape();
			PhysX::RigidStatic& GetActor();
		private:
			ColliderShape mType = ColliderShape::Unknown;
			//PhysX::PhysXMaterial* mMaterial;
			PhysX::PhysXShape mShape;
			PhysX::RigidStatic mStaticActor;
			bool mAddedtoPhysxScene = false;
		};
	}
}