#pragma once
#include <NE_Common.h>

namespace physx { class PxShape; }

namespace Nuclear
{
	namespace PhysX
	{
		//Wraps around physx::PxShape
		class NEAPI PhysXShape
		{
		public:
			PhysXShape();
			~PhysXShape();


			physx::PxShape* mPtr = nullptr; ///< Borrowed by component operations until the owning actor/shape is released.
		};
	}
}