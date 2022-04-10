#pragma once
#include <Core\NE_Common.h>
#include <Engine/Assets/Common.h>
#include <Core\Math\Math.h>
#include <vector>
#include <unordered_map>
#include <Engine/Animation/AnimationClip.h>

namespace NuclearEngine
{
	namespace Assets
	{

		class NEAPI Animations : public Asset<Animations>
		{
		public:
			Animations();
			~Animations();

			std::vector<Animation::AnimationClip> mClips;
		};

	}
}
