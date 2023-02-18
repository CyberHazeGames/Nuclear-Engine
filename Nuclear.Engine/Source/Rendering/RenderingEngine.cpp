#include <Rendering\RenderingEngine.h>
#include <Graphics\Context.h>
#include <Math/Math.h>
#include <Platform\FileSystem.h>
#include <Assets\DefaultMeshes.h>
#include <Diligent/Graphics/GraphicsTools/interface/MapHelper.hpp>
#include <Components\ShaderStructs.h>
#include <Utilities/Logger.h>

namespace Nuclear
{
	namespace Rendering
	{
		using namespace Diligent;

		RenderingEngine& RenderingEngine::GetInstance()
		{
			static RenderingEngine instance;

			return instance;
		}
		bool RenderingEngine::Initialize(const RenderingEngineDesc& desc)
		{
			mDesc = desc;

			if (!InitSceneToScreenPSO())
			{
				NUCLEAR_ERROR("[RenderingEngine] Initialization Failed : InitSceneToScreenPSO() failed!");
				return false;
			}

			Graphics::RenderTargetDesc RTDesc;
			RTDesc.mDimensions = Math::Vector2ui(desc.RTWidth, desc.RTHeight);
			RTDesc.ColorTexFormat = TEX_FORMAT_RGBA16_FLOAT;

			mFinalRT.Create(RTDesc);

			RTDesc.DepthTexFormat = Graphics::Context::GetInstance().GetSwapChain()->GetDesc().DepthBufferFormat;
			mFinalDepthRT.Create(RTDesc);

			NUCLEAR_INFO("[RenderingEngine] RenderingEngine has been initalized succesfully!");
			return true;
		}

		void RenderingEngine::ResizeRTs(Uint32 RTWidth, Uint32 RTHeight)
		{
			Math::Vector2ui newsize(RTWidth, RTHeight);
			mFinalRT.Resize(newsize);
			mFinalDepthRT.Resize(newsize);
		}

		void RenderingEngine::RenderFinalRT()
		{
			auto* RTV = Graphics::Context::GetInstance().GetSwapChain()->GetCurrentBackBufferRTV();
			auto* DSV = Graphics::Context::GetInstance().GetSwapChain()->GetDepthBufferDSV();
			Graphics::Context::GetInstance().GetContext()->SetRenderTargets(1, &RTV, DSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
			Graphics::Context::GetInstance().GetContext()->ClearRenderTarget(RTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
			Graphics::Context::GetInstance().GetContext()->ClearDepthStencil(DSV, CLEAR_DEPTH_FLAG, 1.0f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

			Graphics::Context::GetInstance().GetContext()->SetPipelineState(pSceneToScreenPSO);
			pSceneToScreenSRB->GetVariableByIndex(SHADER_TYPE_PIXEL, 0)->Set(GetFinalRT().GetSRV());
			Graphics::Context::GetInstance().GetContext()->CommitShaderResources(pSceneToScreenSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

			Assets::DefaultMeshes::RenderScreenQuad();
		}

		void RenderingEngine::UpdateCameraCB(Components::CameraComponent* component)
		{
			UpdateCameraCB(component->mCameraData);
		}

		void RenderingEngine::UpdateCameraCB(const Components::CameraBuffer& bufferdata)
		{
			Diligent::MapHelper<Components::CameraBuffer> CBConstants(Graphics::Context::GetInstance().GetContext(), mCameraCB, MAP_WRITE, MAP_FLAG_DISCARD);
			*CBConstants = bufferdata;
		}

		IBuffer* RenderingEngine::GetCameraCB()
		{
			return mCameraCB;
		}

		IBuffer* RenderingEngine::GetAnimationCB()
		{
			return mAnimationCB;
		}
		Graphics::RenderTarget& RenderingEngine::GetFinalRT()
		{
			return mFinalRT;
		}
		Graphics::RenderTarget& RenderingEngine::GetFinalDepthRT()
		{
			return mFinalDepthRT;
		}
		bool RenderingEngine::InitSceneToScreenPSO()
		{
			GraphicsPipelineStateCreateInfo PSOCreateInfo;

			PSOCreateInfo.PSODesc.Name = "SceneToScreen PSO";
			PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
			PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = Graphics::Context::GetInstance().GetSwapChain()->GetDesc().ColorBufferFormat;
			PSOCreateInfo.GraphicsPipeline.BlendDesc.RenderTargets[0].BlendEnable = false;
			PSOCreateInfo.GraphicsPipeline.DSVFormat = Graphics::Context::GetInstance().GetSwapChain()->GetDesc().DepthBufferFormat;
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
				CreationAttribs.Desc.Name = "SceneToScreenVS";

				auto source = Platform::FileSystem::GetInstance().LoadShader("@NuclearAssets@/Shaders/BasicVertex.vs.hlsl", std::set<std::string>(), std::set<std::string>(), true);
				CreationAttribs.Source = source.c_str();
				CreationAttribs.pShaderSourceStreamFactory = Graphics::GraphicsEngine::GetInstance().GetDefaultShaderSourceFactory();

				Graphics::Context::GetInstance().GetDevice()->CreateShader(CreationAttribs, VSShader.RawDblPtr());
			}

			//Create Pixel Shader
			{
				ShaderCreateInfo CreationAttribs;

				CreationAttribs.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
				CreationAttribs.Desc.UseCombinedTextureSamplers = true;
				CreationAttribs.Desc.ShaderType = SHADER_TYPE_PIXEL;
				CreationAttribs.EntryPoint = "main";
				CreationAttribs.Desc.Name = "SceneToScreenPS";

				auto source = Platform::FileSystem::GetInstance().LoadShader("@NuclearAssets@/Shaders/SceneToScreen.ps.hlsl", std::set<std::string>(), std::set<std::string>(), true);
				CreationAttribs.Source = source.c_str();
				Graphics::Context::GetInstance().GetDevice()->CreateShader(CreationAttribs, PSShader.RawDblPtr());
			}

			PSOCreateInfo.pVS = VSShader;
			PSOCreateInfo.pPS = PSShader;
			PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = Graphics::GraphicsEngine::GetInstance().GetRenderToTextureInputLayout().data();
			PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = static_cast<Uint32>(Graphics::GraphicsEngine::GetInstance().GetRenderToTextureInputLayout().size());

			Graphics::PSOResourcesInitInfo ResourcesInitinfo;
			Graphics::GraphicsEngine::GetInstance().InitPSOResources(PSOCreateInfo, ResourcesInitinfo);

			Graphics::Context::GetInstance().GetDevice()->CreateGraphicsPipelineState(PSOCreateInfo, &pSceneToScreenPSO);

			if (!pSceneToScreenPSO)
			{
				return false;
			}


			pSceneToScreenPSO->CreateShaderResourceBinding(pSceneToScreenSRB.RawDblPtr());
			return true;
		}
		RenderingEngine::RenderingEngine()
		{
			{
				BufferDesc CBDesc;
				CBDesc.Name = "AnimationCB";
				CBDesc.Size = sizeof(Math::Matrix4) * 100;
				CBDesc.Usage = USAGE_DYNAMIC;
				CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
				CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
				Graphics::Context::GetInstance().GetDevice()->CreateBuffer(CBDesc, nullptr, &mAnimationCB);
			}

			{
				BufferDesc CBDesc;
				CBDesc.Name = "CameraCB";
				CBDesc.Size = sizeof(Components::CameraBuffer);
				CBDesc.Usage = USAGE_DYNAMIC;
				CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
				CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
				Graphics::Context::GetInstance().GetDevice()->CreateBuffer(CBDesc, nullptr, &mCameraCB);
			}
		}
	}
}