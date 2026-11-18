#include "Components\MeshComponent.h"
#include "Assets\Material.h"
#include "Assets\Shader.h"
#include "Assets\Mesh.h"

#include <Assets/Animations.h>
#include <Animation/Animator.h>
namespace Nuclear 
{
	namespace Components
	{
		MeshComponent::MeshComponent()
		{
			RenderQueue = 1;
			pMesh = nullptr;
			pMaterial = nullptr;
			pAnimator = nullptr;
		}
		MeshComponent::MeshComponent(Assets::Mesh* mesh, Assets::Material* material, Animation::Animator* Animator)
		{
			RenderQueue = 1;
			pMesh = mesh;
			pMaterial = material;
			pAnimator = Animator;
		}
		MeshComponent::~MeshComponent()
		{
			RenderQueue = 1;
			pMesh = nullptr;
			pMaterial = nullptr;
			pAnimator = nullptr;
		}
		void MeshComponent::Update(bool forcedirty)
		{
			if (mDirty)
			{
				if (mEnableRendering)
				{
					bool not_ready = true;
					if (pMesh && pMaterial)
					{
						if (pMesh->GetState() == Assets::IAsset::State::Created	&& 
							pMaterial->GetState() == Assets::IAsset::State::Created)
						{
							if (pMaterial->GetShader())
							{
								if (mMaterialDirty)
								{
									auto assetptr = pMaterial->GetShader();
									mPipelineCntrl.SetPipeline(&assetptr->GetShaderPipeline());
									mMaterialDirty = false;
								}

								if (mRenderSystemHasDefferedPass)
									mRequestDeffered = false;

								if (mRenderSystemHasShadowPass)
									mReceiveShadows = false;

								Uint32 mDefferedPipeline = Utilities::Hash("NE_DEFFERED");
								Uint32 mAnimationHash = Utilities::Hash("NE_ANIMATION");
								Uint32 mRecieveShadowHash = Utilities::Hash("NE_SHADOWS");

								mPipelineCntrl.SetSwitch(mAnimationHash, bool(pAnimator));
								mPipelineCntrl.SetSwitch(mRecieveShadowHash, mReceiveShadows);
								mPipelineCntrl.SetSwitch(mDefferedPipeline, mRequestDeffered);

								for (auto& i : mCustomSwitches)
								{
									mPipelineCntrl.SetSwitch(i.first, i.second);
								}

								if (mPipelineCntrl.Update())									
								{
									RenderQueue = mPipelineCntrl.GetActiveVariant()->mRenderQueue;
									mDirty = false;

									not_ready = false;
								}
							}
						}
					}

					if (not_ready)
					{
						mDirty = true;
						RenderQueue = 0;
					}
				}
				else
				{
					RenderQueue = 0;
				}				
			}
		}

		void MeshComponent::SetVariantSwitch(Uint32 VariantID, bool val)
		{
			if (VariantID == Utilities::Hash("NE_DEFFERED"))
			{
				mRequestDeffered = val;
			}

			mCustomSwitches[VariantID] = val;
			mDirty = true;
		}
		void MeshComponent::SetEnableRendering(bool val)
		{
			if (mEnableRendering != val)
			{
				mEnableRendering = val;
				mDirty = true;
			}
		}
		void MeshComponent::SetCastShadow(bool val)
		{
			if (mCastShadow != val)
			{
				mCastShadow = val;
				mDirty = true;
			}
		}
		void MeshComponent::SetReceiveShadows(bool val)
		{
			if (mReceiveShadows != val)
			{
				mReceiveShadows = val;
				mDirty = true;
			}
		}
        bool MeshComponent::PlayClip(int index, float seconds)
        {
            if (!std::isfinite(seconds) || seconds < 0) return false;
            if (index < 0)
            {
                mOwnedAnimator.reset();
                pAnimator = nullptr;
            }
            else
            {
                auto* clips = pMesh ? pMesh->GetImportedAnimations() : nullptr;
                if (!clips || size_t(index) >= clips->mClips.size()) return false;
                if (!mOwnedAnimator) mOwnedAnimator = std::make_shared<Animation::Animator>();
                mOwnedAnimator->BlendTo(&clips->mClips[index], seconds);
                pAnimator = mOwnedAnimator.get();
            }
            ClipIndex = index;
            BlendSeconds = seconds;
            mDirty = true;
            return true;
        }

        void MeshComponent::AdvanceClip(float seconds)
        {
            if (mOwnedAnimator) mOwnedAnimator->UpdateAnimation(seconds);
        }

        void MeshComponent::SetMesh(Assets::Mesh* mesh)
		{
			if (mesh != pMesh && mOwnedAnimator) PlayClip(-1, 0.f);
			pMesh = mesh;
			mDirty = true;
		}

		void MeshComponent::SetMaterial(Assets::Material* material)
		{
			pMaterial = material;
			mDirty = true;
			mMaterialDirty = true;
		}

		void MeshComponent::SetRenderSystemFlags(bool hasdefferedpass, bool hasshadowpass)
		{
			if (mRenderSystemHasDefferedPass != hasdefferedpass)
			{
				mRenderSystemHasDefferedPass = hasdefferedpass;
				mDirty = true;
			}

			if (mRenderSystemHasShadowPass != hasshadowpass)
			{
				mRenderSystemHasShadowPass = hasshadowpass;
				mDirty = true;
			}
		}
	}
}