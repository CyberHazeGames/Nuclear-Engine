#pragma once
#include <NE_Common.h>

namespace physx { class PxRigidDynamic; }

namespace Nuclear
{
	namespace PhysX
	{
		//Wraps around physx::PxRigidDynamic
		class NEAPI RigidDynamic
		{
		public:
			RigidDynamic();
			~RigidDynamic();

			physx::PxRigidDynamic* mPtr;
		};
	}
}