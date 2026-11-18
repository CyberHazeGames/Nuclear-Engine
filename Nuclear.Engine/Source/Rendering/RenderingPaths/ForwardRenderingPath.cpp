#include "Rendering/RenderingPaths/ForwardRenderingPath.h"
#include <Graphics/GraphicsModule.h>
#include <Components/MeshComponent.h>
#include "Animation/Animator.h"
#include <Assets\Shader.h>
#include <Assets\Texture.h>
#include <Graphics\ShaderPipelineVariant.h>
#include <Rendering/RenderingModule.h>
#include <Rendering\FrameRenderData.h>

namespace Nuclear
{
	namespace Rendering
	{
		void ForwardRenderingPath::StartRendering(FrameRenderData* frame,Graphics::ShaderPipelineVariant* pipeline)
		{
			if (pActivePipeline != pipeline)
			{
				pCurrentFrame = frame;
				pActivePipeline = pipeline;
				Graphics::GraphicsModule::Get().GetContext()->SetPipelineState(pipeline->mPipeline);
				Graphics::GraphicsModule::Get().GetContext()->SetRenderTargets(1, pCurrentFrame->pCamera->GetColorRT().GetRTVDblPtr(), pCurrentFrame->pCamera->GetDepthRT().GetRTV(), Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
			}
		}
			
		void ForwardRenderingPath::Render(Components::MeshComponent& mesh, const Math::Matrix4& modelmatrix)
		{
			pCurrentFrame->pCamera->mCameraData.SurfaceTint = Math::Vector4(mesh.Tint, 1.f);
			pCurrentFrame->pCamera->mCameraData.SurfacePBR = {mesh.Metallic, mesh.Roughness, 0.f, 0.f};
			pCurrentFrame->pCamera->SetModelMatrix(modelmatrix);
			Rendering::RenderingModule::Get().UpdateCameraCB(pCurrentFrame->pCamera);

			UpdateAnimationCB(mesh.GetAnimator());

			////////////////////////      IBL      ///////////////////////////
			for (auto& i : pActivePipeline->mReflection.mIBLTexturesInfo)
			{
				pActivePipeline->mPipelineSRB->GetVariableByIndex(Diligent::SHADER_TYPE_PIXEL, i.mSlot)->Set(i.mTex.pTexture->GetTextureView());
			}

			//Shadows
			////////////////////////     TODO    //////////////////////////////////////
			if (pCurrentFrame->mShadowsEnabled && pActivePipeline->mDesc.isShadowed)
			{
				auto& shadowmapsInfo = pActivePipeline->mReflection.mShadowMapsInfo;
				if (shadowmapsInfo.mDirPos_SMInfo.mType != Assets::ShaderTextureType::Unknown)
				{
					pActivePipeline->mPipelineSRB->GetVariableByIndex(Diligent::SHADER_TYPE_PIXEL, shadowmapsInfo.mDirPos_SMInfo.mSlot)->Set(pCurrentFrame->pDirPosShadowMapSRV);
				}
				if (shadowmapsInfo.mSpot_SMInfo.mType != Assets::ShaderTextureType::Unknown)
				{
					pActivePipeline->mPipelineSRB->GetVariableByIndex(Diligent::SHADER_TYPE_PIXEL, shadowmapsInfo.mSpot_SMInfo.mSlot)->Set(pCurrentFrame->pSpotPosShadowMapSRV);
				}
				if (shadowmapsInfo.mOmniDir_SMInfo.mType != Assets::ShaderTextureType::Unknown)
				{
					pActivePipeline->mPipelineSRB->GetVariableByIndex(Diligent::SHADER_TYPE_PIXEL, shadowmapsInfo.mOmniDir_SMInfo.mSlot)->Set(pCurrentFrame->pOmniDirShadowMapSRV);
				}
			}

			DrawMesh(mesh.GetMesh(), mesh.GetMaterial());
		}
	}
}