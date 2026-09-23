#include "Rendering\RenderPasses\PostProcessingPass.h"
#include <Graphics/GraphicsModule.h>
#include <Diligent/Graphics/GraphicsTools/interface/MapHelper.hpp>
#include <Assets\DefaultMeshes.h>
#include <Rendering/FrameRenderData.h>
#include <Platform\FileSystem.h>
#include <Assets/AssetManager.h>
#include <Assets/Shader.h>
#include <Graphics/GraphicsModule.h>

namespace Nuclear
{
	namespace Rendering
	{
		using namespace Diligent;
		PostProcessingPass::PostProcessingPass()
		{
			_HashedBloomID = Utilities::Hash("BLOOM");
		}

		void PostProcessingPass::Update(FrameRenderData* framedata)
		{
			mPipelineCntrllr.Update();
			auto& pipeline = mPipelineCntrllr.GetActiveVariant()->mPipeline;
			auto& pipelineSRB = mPipelineCntrllr.GetActiveVariant()->mPipelineSRB;

			//Render Skybox

			if (GetBackground().GetSkybox() != nullptr)
			{
				Graphics::GraphicsModule::Get().GetContext()->SetRenderTargets(1, framedata->pCamera->GetColorRT().GetRTVDblPtr(), framedata->pCamera->GetDepthRT().GetRTV(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
				GetBackground().GetSkybox()->Render();
			}

			if (mBloomEnabled)
			{		
				//1 - Extract bloom from scene rt
				Graphics::GraphicsModule::Get().GetContext()->SetPipelineState(pBloomExtractPSO);
				pBloomExtractSRB->GetVariableByIndex(SHADER_TYPE_PIXEL, 0)->Set(framedata->pCamera->GetColorRT().GetSRV());
				Graphics::GraphicsModule::Get().GetContext()->CommitShaderResources(pBloomExtractSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

				Graphics::GraphicsModule::Get().GetContext()->SetRenderTargets(1, BloomRT.GetRTVDblPtr(), nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
				Graphics::GraphicsModule::Get().GetContext()->ClearRenderTarget(BloomRT.GetRTV(), nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

				Assets::DefaultMeshes::RenderScreenQuad();


				//2 - apply bloom blur
				bool horizontal = true, first_iteration = true, horicleared = false, verticleared = false;
				unsigned int amount = 10;

				for (unsigned int i = 0; i < amount; i++)
				{
					Math::Vector4i data;

					if (horizontal)
					{
						mBloomBlur.SetHorizentalPSO(first_iteration ? BloomRT.GetSRV() : mBloomBlur.BlurVerticalRT.GetSRV());
					}
					else
					{
						mBloomBlur.SetVerticalPSO(mBloomBlur.BlurHorizentalRT.GetSRV());
					}

					//X: Texture Width
					//Y: Texutre Height
					//Z: Padding
					//W: Padding
					{
						data.x = BloomRT.GetDimensions().x;
						data.y = BloomRT.GetDimensions().y;

						Diligent::MapHelper<Math::Vector4i> CBConstants(Graphics::GraphicsModule::Get().GetContext(), mBloomBlur.mBlurCB, MAP_WRITE, MAP_FLAG_DISCARD);
						*CBConstants = data;
					}


					if (horizontal)
					{
						Graphics::GraphicsModule::Get().GetContext()->CommitShaderResources(mBloomBlur.mHorzBlurSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
					}
					else
					{
						Graphics::GraphicsModule::Get().GetContext()->CommitShaderResources(mBloomBlur.mVertBlurSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
					}
					Assets::DefaultMeshes::RenderScreenQuad();

					horizontal = !horizontal;
					if (first_iteration)
						first_iteration = false;
				}

				//TODO: Add to DEBUGSYSTEM RTs
				//ImGui::Image(BloomRT.GetSRV(), { 256.f,256.f });
			}

			//3 - apply the remaning effects
			Graphics::GraphicsModule::Get().GetContext()->SetRenderTargets(1, PostFXRT.GetRTVDblPtr(), nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
			Graphics::GraphicsModule::Get().GetContext()->ClearRenderTarget(PostFXRT.GetRTV(), nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

			Graphics::GraphicsModule::Get().GetContext()->SetPipelineState(pipeline);

			pipelineSRB->GetVariableByIndex(SHADER_TYPE_PIXEL, 0)->Set(framedata->pCamera->GetColorRT().GetSRV());
			if (mBloomEnabled)
			{
				pipelineSRB->GetVariableByIndex(SHADER_TYPE_PIXEL, 1)->Set(mBloomBlur.BlurVerticalRT.GetSRV());
			}
			Graphics::GraphicsModule::Get().GetContext()->CommitShaderResources(pipelineSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

			Assets::DefaultMeshes::RenderScreenQuad();


			//4- copy results to camera rt
			CopyTextureAttribs attrib;
			attrib.pSrcTexture = PostFXRT.GetSRV()->GetTexture();
			attrib.pDstTexture = framedata->pCamera->GetColorRT().GetSRV()->GetTexture();
			Graphics::GraphicsModule::Get().GetContext()->CopyTexture(attrib);
		
		}

		void PostProcessingPass::SetPostProcessingEffect(Uint32 effectId, bool value)
		{
			mPipelineCntrllr.SetSwitch(effectId, value);

			//check for bloom
			if (effectId == _HashedBloomID)
			{
				mBloomEnabled = value;
			}

		}

		Graphics::ShaderPipelineSwitchController& PostProcessingPass::GetPipelineController()
		{
			return mPipelineCntrllr;
		}

		void PostProcessingPass::Bake(const PostProcessingBakingDesc& desc)
		{
			mRTWidth = desc.mRTWidth;
			mRTHeight = desc.mRTHeight;
			PostFXRT.Create(desc.mFinalRTDesc);
			BakePostFXPipeline();
		}

		void PostProcessingPass::ResizeRTs(Uint32 Width, Uint32 Height)
		{
			mRTWidth = Width;
			mRTHeight = Height;

			PostFXRT.Resize(Math::Vector2ui(Width, Height));


		//	mPipelineCntrllr.GetActiveVariant()->mPipelineSRB.Release();
		//	mPipelineCntrllr.GetActiveVariant()->mPipeline->CreateShaderResourceBinding(mPipelineCntrllr.GetActiveVariant()->mPipelineSRB, true);
		
		
		//mActiveSRB->GetVariableByIndex(SHADER_TYPE_PIXEL, 0)->Set(SceneRT.GetSRV());

			if (mPipelineCntrllr.GetSwitch(_HashedBloomID).GetValue())
			{
				mBloomBlur.ResizeRenderTargets(mRTWidth, mRTHeight);
				Graphics::RenderTargetDesc RTDesc;
				RTDesc.mDimensions = Math::Vector2ui(mRTWidth, mRTHeight);
				RTDesc.ColorTexFormat = TEX_FORMAT_RGBA16_FLOAT;

				BloomRT.Create(RTDesc);
			}
		}

		void PostProcessingPass::BakePostFXPipeline()
		{
			{
				Assets::ShaderImportingDesc desc;
				desc.mCommonOptions.mLoadOnly = true;

				pPostFXShader = Assets::AssetManager::Get().Import<Assets::Shader>("@NuclearAssets@/Shaders/PostProcessing.NuclearShader");

				Graphics::ShaderPipelineBakingDesc bakedesc;
				bakedesc.pVariantsFactory = &Graphics::GraphicsModule::Get().GetDefaultShaderPipelineVariantFactory();
				bakedesc.mPreVariantPipelineCreationCallback = ([this](Diligent::GraphicsPipelineStateCreateInfo& PSOCreateInfo) {
					PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
					PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = PostFXRT.GetDesc().ColorTexFormat;
					PSOCreateInfo.GraphicsPipeline.DSVFormat = Diligent::TEX_FORMAT_UNKNOWN;
					});



				pPostFXShader->GetShaderPipeline().Bake(bakedesc);
			}

			//bloom
			{
				Graphics::RenderTargetDesc RTDesc;
				RTDesc.mDimensions = Math::Vector2ui(mRTWidth, mRTHeight);

				RTDesc.ColorTexFormat = TEX_FORMAT_RGBA16_FLOAT;

				BloomRT.Create(RTDesc);

				mBloomBlur.Initialize(mRTWidth, mRTHeight);


				//Create bloom extract PSO

				GraphicsPipelineStateCreateInfo PSOCreateInfo;

				PSOCreateInfo.PSODesc.Name = "BloomExtract PSO";
				PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
				PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_RGBA16_FLOAT;
				PSOCreateInfo.GraphicsPipeline.BlendDesc.RenderTargets[0].BlendEnable = false;
				PSOCreateInfo.GraphicsPipeline.DSVFormat = TEX_FORMAT_UNKNOWN;
				PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
				PSOCreateInfo.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = true;
				PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
				PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = false;

				//Create Shaders
				RefCntAutoPtr<IShader> VSShader;
				RefCntAutoPtr<IShader> PSShader;

				//Create Vertex Shader
				{

					ShaderCreateInfo CreationAttribs;

					CreationAttribs.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
					CreationAttribs.Desc.UseCombinedTextureSamplers = true;
					CreationAttribs.Desc.ShaderType = SHADER_TYPE_VERTEX;
					CreationAttribs.EntryPoint = "main";
					CreationAttribs.Desc.Name = "BloomExtractVS";

					auto source = Platform::FileSystem::Get().LoadShader("@NuclearAssets@/Shaders/ScreenSpace.vs.hlsl", std::set<std::string>(), std::set<std::string>(), true);
					CreationAttribs.Source = source.c_str();
					CreationAttribs.pShaderSourceStreamFactory = Graphics::GraphicsModule::Get().GetDefaultShaderSourceFactory();

					Graphics::GraphicsModule::Get().GetDevice()->CreateShader(CreationAttribs, VSShader.RawDblPtr());
				}

				//Create Pixel Shader
				{
					ShaderCreateInfo CreationAttribs;

					CreationAttribs.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
					CreationAttribs.Desc.UseCombinedTextureSamplers = true;
					CreationAttribs.Desc.ShaderType = SHADER_TYPE_PIXEL;
					CreationAttribs.EntryPoint = "main";
					CreationAttribs.Desc.Name = "BloomExtractPS";

					auto source = Platform::FileSystem::Get().LoadShader("@NuclearAssets@/Shaders/BloomExtract.ps.hlsl", std::set<std::string>(), std::set<std::string>(), true);
					CreationAttribs.Source = source.c_str();
					Graphics::GraphicsModule::Get().GetDevice()->CreateShader(CreationAttribs, PSShader.RawDblPtr());
				}

				PSOCreateInfo.pVS = VSShader;
				PSOCreateInfo.pPS = PSShader;
				std::vector<LayoutElement> Layout;

				Layout.push_back(LayoutElement(0, 0, 3, VT_FLOAT32, false));
				Layout.push_back(LayoutElement(1, 0, 2, VT_FLOAT32, false));

				PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = Layout.data();
				PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = static_cast<Uint32>(Layout.size());

				Graphics::PSOResourcesInitInfo ResourcesInitinfo;
				Graphics::GraphicsModule::Get().InitPSOResources(PSOCreateInfo, ResourcesInitinfo);

				Graphics::GraphicsModule::Get().GetDevice()->CreateGraphicsPipelineState(PSOCreateInfo, &pBloomExtractPSO);

				pBloomExtractPSO->CreateShaderResourceBinding(pBloomExtractSRB.RawDblPtr());
			}

			mPipelineCntrllr.SetPipeline(&pPostFXShader->GetShaderPipeline());
			mPipelineCntrllr.Update();
		}


		Rendering::Background& PostProcessingPass::GetBackground()
		{
			return mBackground;
		}

	}
}
