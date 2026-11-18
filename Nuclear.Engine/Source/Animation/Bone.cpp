#define GLM_ENABLE_EXPERIMENTAL
#include "Animation\Bone.h"
#include "Math/glm/gtx/quaternion.hpp"
#include "Utilities/Hash.h"

namespace Nuclear
{
	namespace Animation
	{
		Bone::Bone(const std::string& name, int ID)
			:
			m_Name(name),
			m_ID(ID),
			m_LocalTransform(1.0f),
			mData(BoneData()),
			mHashedName(Utilities::Hash(name))
		{
		}
		inline Bone::Bone(const std::string& name, int ID, BoneData data)
			:
			m_Name(name),
			m_ID(ID),
			m_LocalTransform(1.0f),
			mData(data),
			mHashedName(Utilities::Hash(name))
		{
		}
		Bone::~Bone()
		{
			m_Name = std::string();
			m_ID = 0;
		}
		inline void Bone::Update(float animationTime)
		{
			Math::Matrix4 translation = InterpolatePosition(animationTime);
			Math::Matrix4 rotation = InterpolateRotation(animationTime);
			Math::Matrix4 scale = InterpolateScaling(animationTime);
			m_LocalTransform = translation * rotation * scale;
		}
		inline Math::Matrix4 Bone::GetLocalTransform() { return m_LocalTransform; }
		inline std::string Bone::GetBoneName() const { return m_Name; }
		Uint32 Bone::GetBoneHashedName() const
		{
			return mHashedName;
		}
		inline int Bone::GetBoneID() { return m_ID; }
		inline int Bone::GetPositionIndex(float animationTime)
		{
			for (int index = 0; index < mData.m_NumPositions - 1; ++index)
			{
				if (animationTime < mData.m_Positions[index + 1].timeStamp)
					return index;
			}
			return mData.m_NumPositions - 2;
		}
		inline int Bone::GetRotationIndex(float animationTime)
		{
			for (int index = 0; index < mData.m_NumRotations - 1; ++index)
			{
				if (animationTime < mData.m_Rotations[index + 1].timeStamp)
					return index;
			}
			return mData.m_NumRotations - 2;
		}
		inline int Bone::GetScaleIndex(float animationTime)
		{
			for (int index = 0; index < mData.m_NumScalings - 1; ++index)
			{
				if (animationTime < mData.m_Scales[index + 1].timeStamp)
					return index;
			}
			return mData.m_NumScalings - 2;
		}
		inline float Bone::GetScaleFactor(float lastTimeStamp, float nextTimeStamp, float animationTime)
		{
			float scaleFactor = 0.0f;
			float midWayLength = animationTime - lastTimeStamp;
			float framesDiff = nextTimeStamp - lastTimeStamp;
			scaleFactor = framesDiff > 0 ? glm::clamp(midWayLength / framesDiff, 0.f, 1.f) : 0.f;
			return scaleFactor;
		}
		inline Math::Matrix4 Bone::InterpolatePosition(float animationTime)
		{
			if (mData.m_NumPositions == 0) return Math::Matrix4(1.f);
			if (1 == mData.m_NumPositions)
				return glm::translate(Math::Matrix4(1.0f), mData.m_Positions[0].position);

			int p0Index = GetPositionIndex(animationTime);
			int p1Index = p0Index + 1;
			float scaleFactor = GetScaleFactor(mData.m_Positions[p0Index].timeStamp,
				mData.m_Positions[p1Index].timeStamp, animationTime);
			Math::Vector3 finalPosition = glm::mix(mData.m_Positions[p0Index].position, mData.m_Positions[p1Index].position
				, scaleFactor);
			return glm::translate(Math::Matrix4(1.0f), finalPosition);
		}
		inline Math::Matrix4 Bone::InterpolateRotation(float animationTime)
		{
			if (mData.m_NumRotations == 0) return Math::Matrix4(1.f);
			if (1 == mData.m_NumRotations)
			{
				auto rotation = glm::normalize(mData.m_Rotations[0].orientation);
				return glm::toMat4(rotation);
			}

			int p0Index = GetRotationIndex(animationTime);
			int p1Index = p0Index + 1;
			float scaleFactor = GetScaleFactor(mData.m_Rotations[p0Index].timeStamp,
				mData.m_Rotations[p1Index].timeStamp, animationTime);
			glm::quat finalRotation = glm::slerp(mData.m_Rotations[p0Index].orientation, mData.m_Rotations[p1Index].orientation
				, scaleFactor);
			finalRotation = glm::normalize(finalRotation);
			return glm::toMat4(finalRotation);

		}
		inline Math::Matrix4 Bone::InterpolateScaling(float animationTime)
		{
			if (mData.m_NumScalings == 0) return Math::Matrix4(1.f);
			if (1 == mData.m_NumScalings)
				return glm::scale(Math::Matrix4(1.0f), mData.m_Scales[0].scale);

			int p0Index = GetScaleIndex(animationTime);
			int p1Index = p0Index + 1;
			float scaleFactor = GetScaleFactor(mData.m_Scales[p0Index].timeStamp,
				mData.m_Scales[p1Index].timeStamp, animationTime);
			Math::Vector3 finalScale = glm::mix(mData.m_Scales[p0Index].scale, mData.m_Scales[p1Index].scale
				, scaleFactor);
			return glm::scale(Math::Matrix4(1.0f), finalScale);
		}
	}
}