#include "Engine\Rendering\RenderingPipelines\DefferedRenderingPipeline.h"
#include <Engine\Graphics\Context.h>
#include <Engine\Systems\RenderSystem.h>
#include <Engine\Components/MeshComponent.h>
#include <Engine\Components/EntityInfoComponent.h>
#include "Engine/Animation/Animator.h"
#include <Engine\Components\AnimatorComponent.h>
#include <Diligent/Graphics/GraphicsTools/interface/MapHelper.hpp>
#include <Core\FileSystem.h>
#include <Engine\Systems\CameraSystem.h>
#include <Engine\Systems\DebugSystem.h>
#include <Engine\Assets\Scene.h>

namespace Nuclear
{
	namespace Rendering
	{
        void DefferedRenderingPipeline::BeginFrame(FrameRenderData* frame)
        {
            pCurrentFrame = frame;
            pActiveShadingModel = nullptr;
        }
        void DefferedRenderingPipeline::StartShaderModelRendering(ShadingModel* shadingmodel)
        {
            pActiveShadingModel = shadingmodel;

            //Render To Gbuffer
            Graphics::Context::GetContext()->SetPipelineState(shadingmodel->GetGBufferPipeline());

            std::vector<ITextureView*> RTargets;
            for (auto& i : shadingmodel->mGBuffer.mRenderTargets)
            {
                RTargets.push_back(i.GetRTV());
            }
            Graphics::Context::GetContext()->SetRenderTargets(RTargets.size(), RTargets.data(), pCurrentFrame->mFinalDepthRT.GetRTV(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            for (auto& i : RTargets)
            {
                Graphics::Context::GetContext()->ClearRenderTarget(i, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            }

            Graphics::Context::GetContext()->ClearDepthStencil(pCurrentFrame->mFinalDepthRT.GetRTV(), CLEAR_DEPTH_FLAG, 1.0f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        }
        void DefferedRenderingPipeline::Render(Components::MeshComponent& mesh, const Math::Matrix4& modelmatrix)
        {
            pCurrentFrame->pCamera->SetModelMatrix(modelmatrix);
            pCurrentFrame->pCameraSystemPtr->UpdateBuffer();

            /////////////////////// Animation ////////////////////////////////
            PVoid anim_data;
            Graphics::Context::GetContext()->MapBuffer(pCurrentFrame->pAnimationCB, MAP_WRITE, MAP_FLAG_DISCARD, (PVoid&)anim_data);

            //if (drawcmd.GetAnimator() != nullptr)
            //{
            //    std::vector<Math::Matrix4> ok;
            //    ok.reserve(100);

            //    auto transforms = drawcmd.GetAnimator()->mAnimator->GetFinalBoneMatrices();
            //    for (int i = 0; i < transforms.size(); ++i)
            //    {
            //        ok.push_back(transforms[i]);
            //    }

            //    anim_data = memcpy(anim_data, ok.data(), ok.size() * sizeof(Math::Matrix4));
            //}
            //else {
                Math::Matrix4 empty(0.0f);
                anim_data = memcpy(anim_data, &empty, sizeof(Math::Matrix4));
          //  }

            Graphics::Context::GetContext()->UnmapBuffer(pCurrentFrame->pAnimationCB, MAP_WRITE);

            InstantRender(mesh.mMesh, mesh.mMaterial);
        }
        void DefferedRenderingPipeline::FinishShaderModelRendering()
        {
            //Apply Lighting
            Graphics::Context::GetContext()->SetPipelineState(pActiveShadingModel->GetShadersPipeline());
            Graphics::Context::GetContext()->SetRenderTargets(1, pCurrentFrame->mFinalRT.GetRTVDblPtr(), nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            for (int i = 0; i < pActiveShadingModel->mGBuffer.mRenderTargets.size(); i++)
            {
                pActiveShadingModel->GetShadersPipelineSRB()->GetVariableByIndex(SHADER_TYPE_PIXEL, i)->Set(pActiveShadingModel->mGBuffer.mRenderTargets.at(i).GetSRV());
            }

            //IBL
            for (int i = 0; i < pActiveShadingModel->mIBLTexturesInfo.size(); i++)
            {
                pActiveShadingModel->GetShadersPipelineSRB()->GetVariableByIndex(SHADER_TYPE_PIXEL, pActiveShadingModel->mIBLTexturesInfo.at(i).mSlot)->Set(pActiveShadingModel->mIBLTexturesInfo.at(i).mTex.GetImage()->mTextureView);
            }

            Graphics::Context::GetContext()->CommitShaderResources(pActiveShadingModel->GetShadersPipelineSRB(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            Assets::DefaultMeshes::RenderScreenQuad();

            pActiveShadingModel = nullptr;

            //Send GBUFFER to DebugSystem
            //if (renderer->mScene->GetSystemManager().GetSystem<Systems::DebugSystem>())
            //{
            //    for (auto& i : mGBuffer.mRenderTargets)
            //    {
            //        renderer->mScene->GetSystemManager().GetSystem<Systems::DebugSystem>()->mRegisteredRTs.push_back(&i);
            //    }
            //}
            //mGBuffer.DebugIMGUI();
        }
    }
}