#include <Engine\Graphics\RenderingPipelines\BlinnPhong.h>
#include <Engine\Graphics\Context.h>
#include <Core\FileSystem.h>

namespace NuclearEngine
{
	namespace Graphics
	{
		BlinnPhong::BlinnPhong(bool NormalMaps)
		{
			if(NormalMaps)
				mID = Utilities::Hash("NE_BlinnPhong");
			else
				mID = Utilities::Hash("NE_BlinnPhong_Normal");

			mNormalMaps = NormalMaps;
		}
		bool BlinnPhong::Bake(const RenderingPipelineDesc& desc)
		{
			PipelineStateDesc PSODesc;

			PSODesc.Name = "RenderSystem PSO";
			PSODesc.IsComputePipeline = false;
			PSODesc.GraphicsPipeline.NumRenderTargets = 1;
			PSODesc.GraphicsPipeline.RTVFormats[0] = Graphics::Context::GetSwapChain()->GetDesc().ColorBufferFormat;
			PSODesc.GraphicsPipeline.BlendDesc.RenderTargets[0].BlendEnable = false;
			PSODesc.GraphicsPipeline.DSVFormat = Graphics::Context::GetSwapChain()->GetDesc().DepthBufferFormat;
			PSODesc.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			PSODesc.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = true;
			PSODesc.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
			PSODesc.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

			//Create Shaders
			RefCntAutoPtr<IShader> VSShader;
			RefCntAutoPtr<IShader> PSShader;
			std::vector<LayoutElement> LayoutElems;

			//Create Vertex Shader
			Managers::AutoVertexShaderDesc VertShaderDesc;
			VertShaderDesc.Name = "BlinnPhongVS";
			VertShaderDesc.InTangents = true;

			VSShader = Graphics::GraphicsEngine::GetShaderManager()->CreateAutoVertexShader(VertShaderDesc, &LayoutElems);


			//Create Pixel Shader
			{
				ShaderCreateInfo CreationAttribs;
				CreationAttribs.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
				CreationAttribs.UseCombinedTextureSamplers = true;
				CreationAttribs.Desc.ShaderType = SHADER_TYPE_PIXEL;
				CreationAttribs.EntryPoint = "main";
				CreationAttribs.Desc.Name = "BlinnPhongPS";


				std::vector<std::string> defines;

				if (desc.DirLights > 0) { defines.push_back("NE_DIR_LIGHTS_NUM " + std::to_string(desc.DirLights)); }
				if (desc.PointLights > 0) { defines.push_back("NE_POINT_LIGHTS_NUM " + std::to_string(desc.PointLights)); }
				if (desc.SpotLights > 0) { defines.push_back("NE_SPOT_LIGHTS_NUM " + std::to_string(desc.SpotLights)); }
				if (mNormalMaps) { defines.push_back("NE_USE_NORMAL_MAPS"); }

				auto source = Core::FileSystem::LoadShader("Assets/NuclearEngine/Shaders/BlinnPhong.ps.hlsl", defines, std::vector<std::string>(), true);
				CreationAttribs.Source = source.c_str();
				Graphics::Context::GetDevice()->CreateShader(CreationAttribs, &PSShader);
			}

			PSODesc.GraphicsPipeline.pVS = VSShader;
			PSODesc.GraphicsPipeline.pPS = PSShader;
			PSODesc.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems.data();
			PSODesc.GraphicsPipeline.InputLayout.NumElements = LayoutElems.size();
			auto Vars = Graphics::GraphicsEngine::GetShaderManager()->ReflectShaderVariables(VSShader, PSShader);
			Graphics::GraphicsEngine::GetShaderManager()->ProcessAndCreatePipeline(&mPipeline, PSODesc, Vars, true);


			if (desc.CameraBufferPtr)
				mPipeline->GetStaticVariableByName(SHADER_TYPE_VERTEX, "NEStatic_Camera")->Set(desc.CameraBufferPtr);

			if (desc.LightsBufferPtr)
				mPipeline->GetStaticVariableByName(SHADER_TYPE_PIXEL, "NEStatic_Lights")->Set(desc.LightsBufferPtr);

			ReflectPixelShaderData();

			mStatus = BakeStatus::Baked;

			return true;
		}
	}
}