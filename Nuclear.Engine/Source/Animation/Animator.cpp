#include "Animation/Animator.h"
#include <Assets/Animations.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <Math/glm/gtx/matrix_decompose.hpp>
#include <algorithm>
namespace Nuclear
{
	namespace Animation
	{
		Animator::Animator()
		{
			m_CurrentAnimation = nullptr;
			m_CurrentTime = 0;
			m_DeltaTime = 0;
		}
		Animator::Animator(Animation::AnimationClip* animation)
		{
			Initialize(animation);
		}
		void Animator::Initialize(Assets::Animations* animation)
		{
			if (animation)
			{
				if (animation->mClips.size() > 0)
				{
					Initialize(&animation->mClips.at(0));
				}
			}
		}
		void Animator::Initialize(Animation::AnimationClip* animation)
		{
			m_CurrentTime = 0.0;
			m_CurrentAnimation = animation;

			m_FinalBoneMatrices.assign(100, Math::Matrix4(1.f));
			mFromPose.clear();
			mPose.clear();
		}
		void Animator::UpdateAnimation(float dt)
		{
			m_DeltaTime = dt;
			if (m_CurrentAnimation && m_CurrentAnimation->GetDuration() > 0)
			{
				m_CurrentTime += m_CurrentAnimation->GetTicksPerSecond() * dt;
				m_CurrentTime = fmod(m_CurrentTime, m_CurrentAnimation->GetDuration());
				mBlendElapsed = std::min(mBlendElapsed + std::max(dt, 0.f), mBlendSeconds);
				if (mBlendElapsed >= mBlendSeconds) mFromPose.clear();
				CalculateBoneTransform(&m_CurrentAnimation->GetRootNode(), Math::Matrix4(1.0f));
			}
		}
		void Animator::PlayAnimation(Animation::AnimationClip* pAnimation)
		{
			mFromPose.clear();
			m_CurrentAnimation = pAnimation;
			m_CurrentTime = 0.0f;
		}
		void Animator::BlendTo(AnimationClip* clip, float seconds)
        {
            if (m_FinalBoneMatrices.empty()) Initialize(clip);
            PlayAnimation(clip);
            mBlendElapsed = 0;
            mBlendSeconds = std::max(0.f, seconds);
            if (mBlendSeconds > 0) mFromPose = mPose;
        }

        void Animator::CalculateBoneTransform(const Animation::ClipNodeData* node, Math::Matrix4 parentTransform)
		{
			Uint32 nodehash = node->mHashedName;
			Math::Matrix4 nodeTransform = node->transformation;

			Bone* Bone = m_CurrentAnimation->FindBone(nodehash);

			if (Bone)
			{
				Bone->Update(m_CurrentTime);
				nodeTransform = Bone->GetLocalTransform();
			}

			auto previous = mFromPose.find(nodehash);
            if (previous != mFromPose.end())
            {
                Math::Vector3 oldScale, newScale, oldPos, newPos, skew;
                Math::Vector4 perspective;
                Math::Quaternion oldRot, newRot;
                if (glm::decompose(previous->second, oldScale, oldRot, oldPos, skew, perspective) &&
                    glm::decompose(nodeTransform, newScale, newRot, newPos, skew, perspective))
                {
                    float t = mBlendElapsed / mBlendSeconds;
                    nodeTransform = glm::translate(Math::Matrix4(1.f), glm::mix(oldPos, newPos, t)) *
                        glm::mat4_cast(glm::slerp(oldRot, newRot, t)) * glm::scale(Math::Matrix4(1.f), glm::mix(oldScale, newScale, t));
                }
            }
            mPose[nodehash] = nodeTransform;
            Math::Matrix4 globalTransformation = parentTransform * nodeTransform;

			const auto& boneInfoMap = m_CurrentAnimation->GetBoneIDMap();
			if (boneInfoMap.find(nodehash) != boneInfoMap.end())
			{
				int index = boneInfoMap.at(nodehash).id;
				Math::Matrix4 offset = boneInfoMap.at(nodehash).offset;
				if (index >= 0 && size_t(index) < m_FinalBoneMatrices.size()) m_FinalBoneMatrices[index] = globalTransformation * offset;
			}

			for (int i = 0; i < node->childrenCount; i++)
				CalculateBoneTransform(&node->children[i], globalTransformation);
		}
		inline std::vector<Math::Matrix4> Animator::GetFinalBoneMatrices()
		{
			return m_FinalBoneMatrices;
		}
	}
}