#include "ECS/Transform.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <Math/glm/gtx/quaternion.hpp>
namespace Nuclear
{
	namespace ECS
	{
		//Transform::Transform(Math::Matrix4 Transform)
		//{
		//	mDirty = true;
		//	mWorldMatrix = Transform;
		//}

		Transform::Transform(const Math::Vector3& position, const Math::Vector3& scale, const Math::Quaternion& rotation)
		{
			mWorldMatrix = Math::Matrix4(1.0f);
			mLocalPosition = position;
			mLocalRotation = rotation;
			mLocalScale = scale;
			mDirty = ALL;
			mWorldPosition = Math::Vector3(0.0f);
			Update();
		}

		Transform::~Transform()
		{
		}

		void Transform::SetPosition(const Math::Vector3& position)
		{
			mLocalPosition = position;
			mDirty = TRANSLATION;
			Update();
		}

		void Transform::SetRotation(const Math::Quaternion& rotation)
		{
			mLocalRotation = rotation;
			mDirty = ROTATION;
		}

		void Transform::SetRotation(const Math::Vector3& axis, float angle)
		{
			mLocalRotation = glm::angleAxis(glm::degrees(angle), axis);
			mDirty = ROTATION;
		}

		void Transform::SetRotation(const Math::Vector3& eular)
		{
			mLocalRotation = Math::Quaternion(eular);
			mDirty = ROTATION;
		}

		void Transform::SetScale(const Math::Vector3& scale)
		{
			mLocalScale = scale;
			mDirty = SCALE;
		}

		void Transform::SetScale(float scale)
		{
			SetScale(Math::Vector3(scale));
		}

		Math::Vector3 Transform::GetLocalPosition()
		{
			return mLocalPosition;
		}

		Math::Quaternion Transform::GetLocalRotation()
		{
			return mLocalRotation;
		}

		Math::Vector3 Transform::GetLocalRotationEular()
		{
			return glm::eulerAngles(mLocalRotation);
		}

		Math::Vector3 Transform::GetLocalScale()
		{
			return mLocalScale;
		}

		Math::Vector3 Transform::GetWorldPosition()
		{
			return mWorldPosition;
		}

		Math::Matrix4 Transform::GetWorldMatrix()
		{
			return mWorldMatrix;
		}

		void Transform::Update()
		{

			Math::Matrix4 model(1.0f);
			model = Math::translate(model, mLocalPosition);
			model = Math::scale(model, mLocalScale);
			model = model * Math::toMat4(mLocalRotation);

			mWorldMatrix = model;

			mWorldPosition = mWorldMatrix[3];

			mDirty = TransformChanged::NONE;
		}

		

		//Math::Vector3 Transform::GetWorldScale()
		//{
		//	Math::Matrix4 transform = GetTransform();
		//	Math::Vector3 scale = Math::Vector3(transform[0][0], transform[1][1], transform[2][2]);
		//	if (scale.x < 0.0f) scale.x *= -1.0f;
		//	if (scale.y < 0.0f) scale.y *= -1.0f;
		//	if (scale.z < 0.0f) scale.z *= -1.0f;
		//	mWorldScale = scale;
		//	return mWorldScale;
		//}
	}
}