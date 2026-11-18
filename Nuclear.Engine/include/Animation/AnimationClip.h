#pragma once
#include <Animation/BoneInfo.h>
#include <Animation/Bone.h>
#include <vector>
#include <unordered_map>

namespace Nuclear
{
	namespace Animation
	{
		struct ClipNodeData
		{
			~ClipNodeData()
			{
				childrenCount = 0;
				mName = std::string();
				children.clear();
			}
			Math::Matrix4 transformation;
			std::string mName;
			Uint32 mHashedName;
			int childrenCount;
			std::vector<ClipNodeData> children;
		};
		class NEAPI AnimationClip
		{
		public:
			AnimationClip();
			~AnimationClip();

			Animation::Bone* FindBone(const std::string& name);
			Animation::Bone* FindBone(Uint32 hashedname);


			inline Float64 GetTicksPerSecond() { return m_TicksPerSecond; }
			inline Float64 GetDuration() { return m_Duration; }

			inline const ClipNodeData& GetRootNode() { return mRootNode; }
			inline const std::unordered_map<Uint32, Animation::BoneInfo>& GetBoneIDMap()
			{
				return mBoneInfoMap;
			}

			//private:

			std::string Name; ///< Clip name assigned by the source model.
			Float64 m_Duration;
			Float64 m_TicksPerSecond;
			std::vector<Animation::Bone> mBones;
			ClipNodeData mRootNode;
			std::unordered_map<Uint32, Animation::BoneInfo> mBoneInfoMap;

		};
	}
}