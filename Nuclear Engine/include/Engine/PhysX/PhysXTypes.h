#pragma once
#include <Base\NE_Common.h>
#include <PhysX/include/PxPhysicsAPI.h>
#include <PhysX/include/foundation/PxVec3.h>
#include <PhysX/include/foundation/PxVec4.h>
#include <PhysX/include/foundation/PxTransform.h>
#include <Base/Math/Math.h>

namespace NuclearEngine
{
	namespace PhysX
	{
		inline const physx::PxVec3& To(const Math::Vector3& vec)
		{
			return *(physx::PxVec3*) & vec;
		}

		inline const physx::PxVec4& To(const Math::Vector4& vec)
		{
			return *(physx::PxVec4*) & vec;
		}

		inline const physx::PxMat44& To(const Math::Matrix4& mat)
		{
			return *(physx::PxMat44*) & mat;
		}

		inline const  Math::Vector3& From(const physx::PxVec3& vec)
		{
			return *(Math::Vector3*)& vec;
		}

		inline const  Math::Vector4& From(const physx::PxVec4& vec)
		{
			return *(Math::Vector4*)& vec;
		}

		inline const Math::Matrix4& From(const physx::PxMat44& mat)
		{
			return *(Math::Matrix4*) & mat;
		}
	}
}