#include <Rendering\RenderingModule.h>
#include <Graphics/GraphicsModule.h>
#include <Math/Math.h>
#include <Platform\FileSystem.h>
#include <Assets\DefaultMeshes.h>
#include <Diligent/Graphics/GraphicsTools/interface/MapHelper.hpp>
#include <Components\ShaderStructs.h>
#include <Utilities/Logger.h>
#include <Graphics/GraphicsModule.h>

namespace Nuclear
{
	namespace Rendering
	{
		using namespace Diligent;

		RenderingModule& RenderingModule::Get() { static RenderingModule instance; return instance; }
		bool RenderingModule::OnInitialize() { return Initialize(mStartupDesc); }
		bool RenderingModule::Initialize(const RenderingModuleDesc& desc)
		{
			mDesc = desc;
			{
				BufferDesc CBDesc;
				CBDesc.Name = "AnimationCB";
				CBDesc.Size = sizeof(Math::Matrix4) * 100;
				CBDesc.Usage = USAGE_DYNAMIC;
				CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
				CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
				Graphics::GraphicsModule::Get().GetDevice()->CreateBuffer(CBDesc, nullptr, &mAnimationCB);
			}

			{
				BufferDesc CBDesc;
				CBDesc.Name = "CameraCB";
				CBDesc.Size = sizeof(Components::CameraBuffer);
				CBDesc.Usage = USAGE_DYNAMIC;
				CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
				CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
				Graphics::GraphicsModule::Get().GetDevice()->CreateBuffer(CBDesc, nullptr, &mCameraCB);
			}
			if (!InitSceneToScreenPSO())
			{
				NUCLEAR_ERROR("[RenderingModule] Initialization Failed : InitSceneToScreenPSO() failed!");
				return false;
			}

			Graphics::RenderTargetDesc RTDesc;
			RTDesc.mDimensions = Math::Vector2ui(desc.RTWidth, desc.RTHeight);
			RTDesc.ColorTexFormat = TEX_FORMAT_RGBA16_FLOAT;

			mFinalRT.Create(RTDesc);

			RTDesc.DepthTexFormat = Graphics::GraphicsModule::Get().GetSwapChain()->GetDesc().DepthBufferFormat;
			mFinalDepthRT.Create(RTDesc);

			NUCLEAR_INFO("[RenderingModule] RenderingModule has been initialized succesfully!");
			return true;
		}

		void RenderingModule::Shutdown()
		{
			mCameraCB.Release();;
			mAnimationCB.Release();

			pSceneToScreenPSO.Release();
			pSceneToScreenSRB.Release();

			mFinalRT.Release();
			mFinalDepthRT.Release();
		}

		void RenderingModule::ResizeRTs(Uint32 RTWidth, Uint32 RTHeight)
		{
			Math::Vector2ui newsize(RTWidth, RTHeight);
			mFinalRT.Resize(newsize);
			mFinalDepthRT.Resize(newsize);
		}

		void RenderingModule::RenderFinalRT()
		{
			auto* RTV = Graphics::GraphicsModule::Get().GetSwapChain()->GetCurrentBackBufferRTV();
			auto* DSV = Graphics::GraphicsModule::Get().GetSwapChain()->GetDepthBufferDSV();
			Graphics::GraphicsModule::Get().GetContext()->SetRenderTargets(1, &RTV, DSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
			Graphics::GraphicsModule::Get().GetContext()->ClearRenderTarget(RTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
			Graphics::GraphicsModule::Get().GetContext()->ClearDepthStencil(DSV, CLEAR_DEPTH_FLAG, 1.0f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

			Graphics::GraphicsModule::Get().GetContext()->SetPipelineState(pSceneToScreenPSO);
			pSceneToScreenSRB->GetVariableByIndex(SHADER_TYPE_PIXEL, 0)->Set(GetFinalRT().GetSRV());
			Graphics::GraphicsModule::Get().GetContext()->CommitShaderResources(pSceneToScreenSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

			Assets::DefaultMeshes::RenderScreenQuad();
		}

		void RenderingModule::UpdateCameraCB(Components::CameraComponent* component)
		{
			UpdateCameraCB(component->mCameraData);
		}

		void RenderingModule::UpdateCameraCB(const Components::CameraBuffer& bufferdata)
		{
			Diligent::MapHelper<Components::CameraBuffer> CBConstants(Graphics::GraphicsModule::Get().GetContext(), mCameraCB, MAP_WRITE, MAP_FLAG_DISCARD);
			*CBConstants = bufferdata;
		}

		IBuffer* RenderingModule::GetCameraCB()
		{
			return mCameraCB;
		}

		IBuffer* RenderingModule::GetAnimationCB()
		{
			return mAnimationCB;
		}
		Graphics::RenderTarget& RenderingModule::GetFinalRT()
		{
			return mFinalRT;
		}
		Graphics::RenderTarget& RenderingModule::GetFinalDepthRT()
		{
			return mFinalDepthRT;
		}
		bool RenderingModule::InitSceneToScreenPSO()
		{
			GraphicsPipelineStateCreateInfo PSOCreateInfo;

			PSOCreateInfo.PSODesc.Name = "SceneToScreen PSO";
			PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
			PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = Graphics::GraphicsModule::Get().GetSwapChain()->GetDesc().ColorBufferFormat;
			PSOCreateInfo.GraphicsPipeline.BlendDesc.RenderTargets[0].BlendEnable = false;
			PSOCreateInfo.GraphicsPipeline.DSVFormat = Graphics::GraphicsModule::Get().GetSwapChain()->GetDesc().DepthBufferFormat;
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

				auto source = Platform::FileSystem::Get().LoadShader("@NuclearAssets@/Shaders/BasicVertex.vs.hlsl", std::set<std::string>(), std::set<std::string>(), true);
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
				CreationAttribs.Desc.Name = "SceneToScreenPS";

				auto source = Platform::FileSystem::Get().LoadShader("@NuclearAssets@/Shaders/SceneToScreen.ps.hlsl", std::set<std::string>(), std::set<std::string>(), true);
				CreationAttribs.Source = source.c_str();
				Graphics::GraphicsModule::Get().GetDevice()->CreateShader(CreationAttribs, PSShader.RawDblPtr());
			}

			PSOCreateInfo.pVS = VSShader;
			PSOCreateInfo.pPS = PSShader;
			PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = Graphics::GraphicsModule::Get().GetRenderToTextureInputLayout().data();
			PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = static_cast<Uint32>(Graphics::GraphicsModule::Get().GetRenderToTextureInputLayout().size());

			Graphics::PSOResourcesInitInfo ResourcesInitinfo;
			Graphics::GraphicsModule::Get().InitPSOResources(PSOCreateInfo, ResourcesInitinfo);

			Graphics::GraphicsModule::Get().GetDevice()->CreateGraphicsPipelineState(PSOCreateInfo, &pSceneToScreenPSO);

			if (!pSceneToScreenPSO)
			{
				return false;
			}


			pSceneToScreenPSO->CreateShaderResourceBinding(pSceneToScreenSRB.RawDblPtr());
			return true;
		}
		RenderingModule::RenderingModule()
		{

		}
	}
}
