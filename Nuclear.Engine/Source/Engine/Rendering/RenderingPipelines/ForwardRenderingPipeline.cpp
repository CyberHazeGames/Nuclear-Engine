#include "Engine/Rendering/RenderingPipelines/ForwardRenderingPipeline.h"
#include <Engine\Graphics\Context.h>
#include <Engine\Components/MeshComponent.h>
#include <Engine\Components/EntityInfoComponent.h>
#include "Engine/Animation/Animator.h"
#include <Engine\Components\SpotLightComponent.h>
#include <Engine\Systems\RenderSystem.h>
#include <Engine\Systems\CameraSystem.h>
#include <Engine\Assets\Scene.h>
#include <Core/Logger.h>

namespace Nuclear
{
	namespace Rendering
	{
		void ForwardRenderingPipeline::BeginFrame(FrameRenderData* frame)
		{
			pCurrentFrame = frame;
			pActiveShadingModel = nullptr;
		}
		void ForwardRenderingPipeline::StartStaticShaderModelRendering(ShadingModel* shadingmodel)
		{
			pActiveShadingModel = shadingmodel;
			Graphics::Context::GetContext()->SetPipelineState(shadingmodel->GetShadersPipeline());
			Graphics::Context::GetContext()->SetRenderTargets(1, pCurrentFrame->mFinalRT.GetRTVDblPtr(), pCurrentFrame->mFinalDepthRT.GetRTV(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		}
		void ForwardRenderingPipeline::Render(Components::MeshComponent& mesh, const Math::Matrix4& modelmatrix)
		{
			pCurrentFrame->pCamera->SetModelMatrix(modelmatrix);
			pCurrentFrame->pCameraSystemPtr->UpdateBuffer();

			////////////////////////      IBL      ///////////////////////////
			for (int i = 0; i < pActiveShadingModel->mIBLTexturesInfo.size(); i++)
			{
				pActiveShadingModel->GetShadersPipelineSRB()->GetVariableByIndex(SHADER_TYPE_PIXEL, pActiveShadingModel->mIBLTexturesInfo.at(i).mSlot)->Set(pActiveShadingModel->mIBLTexturesInfo.at(i).mTex.GetImage()->mTextureView);
			}

			//Shadows
			////////////////////////     TODO    //////////////////////////////////////
			if (pCurrentFrame->mShadowsEnabled)
			{
				if (pActiveShadingModel->mDirPos_ShadowmapInfo.mType != Assets::ShaderTextureType::Unknown)
				{
					pActiveShadingModel->GetShadersPipelineSRB()->GetVariableByIndex(SHADER_TYPE_PIXEL, pActiveShadingModel->mDirPos_ShadowmapInfo.mSlot)->Set(pCurrentFrame->pDirPosShadowMapSRV);
				}
				if (pActiveShadingModel->mSpot_ShadowmapInfo.mType != Assets::ShaderTextureType::Unknown)
				{
					pActiveShadingModel->GetShadersPipelineSRB()->GetVariableByIndex(SHADER_TYPE_PIXEL, pActiveShadingModel->mSpot_ShadowmapInfo.mSlot)->Set(pCurrentFrame->pSpotPosShadowMapSRV);
				}
				if (pActiveShadingModel->mOmniDir_ShadowmapInfo.mType != Assets::ShaderTextureType::Unknown)
				{
					pActiveShadingModel->GetShadersPipelineSRB()->GetVariableByIndex(SHADER_TYPE_PIXEL, pActiveShadingModel->mOmniDir_ShadowmapInfo.mSlot)->Set(pCurrentFrame->pOmniDirShadowMapSRV);
				}
			}

			RenderStaticMesh(mesh.mMesh, mesh.mMaterial);
		}
		void ForwardRenderingPipeline::Render(Components::SkinnedMeshComponent& mesh, const Math::Matrix4& modelmatrix)
		{
			pCurrentFrame->pCamera->SetModelMatrix(modelmatrix);
			pCurrentFrame->pCameraSystemPtr->UpdateBuffer();

			/////////////////////// Animation ////////////////////////////////
			PVoid anim_data;
			Graphics::Context::GetContext()->MapBuffer(pCurrentFrame->pAnimationCB, MAP_WRITE, MAP_FLAG_DISCARD, (PVoid&)anim_data);

			if (mesh.mAnimator != nullptr)
			{
				std::vector<Math::Matrix4> ok;
				ok.reserve(100);

				auto transforms = mesh.mAnimator->GetFinalBoneMatrices();
				for (int i = 0; i < transforms.size(); ++i)
				{
					ok.push_back(transforms[i]);
				}

				anim_data = memcpy(anim_data, ok.data(), ok.size() * sizeof(Math::Matrix4));
			}
			else
			{
				
				NUCLEAR_ERROR("[ForwardRenderingPipeline] Rendering SkinnedMeshComponent with no animator...");
				Math::Matrix4 empty(0.0f);
				anim_data = memcpy(anim_data, &empty, sizeof(Math::Matrix4));
			}


			Graphics::Context::GetContext()->UnmapBuffer(pCurrentFrame->pAnimationCB, MAP_WRITE);

			////////////////////////      IBL      ///////////////////////////
			for (int i = 0; i < pActiveShadingModel->mIBLTexturesInfo.size(); i++)
			{
				pActiveShadingModel->GetSkinnedShadersPipelineSRB()->GetVariableByIndex(SHADER_TYPE_PIXEL, pActiveShadingModel->mIBLTexturesInfo.at(i).mSlot)->Set(pActiveShadingModel->mIBLTexturesInfo.at(i).mTex.GetImage()->mTextureView);
			}

			//Shadows
			////////////////////////     TODO    //////////////////////////////////////
			if (pCurrentFrame->mShadowsEnabled)
			{
				if (pActiveShadingModel->mDirPos_ShadowmapInfo.mType != Assets::ShaderTextureType::Unknown)
				{
					pActiveShadingModel->GetSkinnedShadersPipelineSRB()->GetVariableByIndex(SHADER_TYPE_PIXEL, pActiveShadingModel->mDirPos_ShadowmapInfo.mSlot)->Set(pCurrentFrame->pDirPosShadowMapSRV);
				}
				if (pActiveShadingModel->mSpot_ShadowmapInfo.mType != Assets::ShaderTextureType::Unknown)
				{
					pActiveShadingModel->GetSkinnedShadersPipelineSRB()->GetVariableByIndex(SHADER_TYPE_PIXEL, pActiveShadingModel->mSpot_ShadowmapInfo.mSlot)->Set(pCurrentFrame->pSpotPosShadowMapSRV);
				}
				if (pActiveShadingModel->mOmniDir_ShadowmapInfo.mType != Assets::ShaderTextureType::Unknown)
				{
					pActiveShadingModel->GetSkinnedShadersPipelineSRB()->GetVariableByIndex(SHADER_TYPE_PIXEL, pActiveShadingModel->mOmniDir_ShadowmapInfo.mSlot)->Set(pCurrentFrame->pOmniDirShadowMapSRV);
				}
			}

			RenderSkinnedMesh(mesh.mMesh, mesh.mMaterial);
		}
		void ForwardRenderingPipeline::FinishStaticShaderModelRendering()
		{
			pActiveShadingModel = nullptr;
			//Do nothing
		}
		void ForwardRenderingPipeline::StartSkinnedShaderModelRendering(ShadingModel* shadingmodel)
		{
			pActiveShadingModel = shadingmodel;
			Graphics::Context::GetContext()->SetPipelineState(shadingmodel->GetSkinnedShadersPipeline());
			Graphics::Context::GetContext()->SetRenderTargets(1, pCurrentFrame->mFinalRT.GetRTVDblPtr(), pCurrentFrame->mFinalDepthRT.GetRTV(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		}
	}
}