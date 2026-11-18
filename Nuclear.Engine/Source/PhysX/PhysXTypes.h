#pragma once
#include <NE_Common.h>
#include <PhysX/include/PxPhysicsAPI.h>
#include <foundation/PxVec3.h>
#include <foundation/PxVec4.h>
#include <foundation/PxTransform.h>
#include <ECS\Transform.h>
#include <Math/Math.h>

namespace Nuclear
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
		inline const physx::PxQuat& To(const Math::Quaternion& quat)
		{
			return *(physx::PxQuat*) & quat;
		}

		inline physx::PxTransform To(const Math::Vector3& pos, const Math::Quaternion& quat)
		{
			return physx::PxTransform(To(pos), To(quat));
		}

		inline physx::PxTransform To(ECS::Transform& trans)
		{
			auto rotation = trans.GetWorldRotation();
			if (Math::dot(rotation, rotation) < .000001f) rotation = Math::Quaternion(1, 0, 0, 0);
			return physx::PxTransform(To(trans.GetWorldPosition()), To(Math::normalize(rotation)));
		}

		inline const  Math::Vector3& From(const physx::PxVec3& vec)
		{
			return *(Math::Vector3*)& vec;
		}

		inline const  Math::Quaternion& From(const physx::PxQuat& vec)
		{
			return *(Math::Quaternion*)&vec;
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