#include "Rendering/Skybox.h"
#include <Graphics/GraphicsModule.h>
#include <Assets\AssetManager.h>
#include <Components/CameraComponent.h>
#include "Graphics/GraphicsModule.h"
#include <Diligent/Graphics/GraphicsEngine/interface/Texture.h>
#include <Platform/FileSystem.h>
#include <Rendering/RenderingModule.h>
#include <Assets\Texture.h>

namespace Nuclear
{
	namespace Rendering
	{
		using namespace Diligent;
		float skyboxVertices[] = {
			-1.0f, -1.0f, -1.0f,
			1.0f, -1.0f, -1.0f,
			1.0f,  1.0f, -1.0f,
			1.0f,  1.0f, -1.0f,
			-1.0f,  1.0f, -1.0f, 
			-1.0f, -1.0f, -1.0f,

			-1.0f, -1.0f,  1.0f,
			1.0f, -1.0f,  1.0f,
			1.0f,  1.0f,  1.0f,
			1.0f,  1.0f,  1.0f,
			-1.0f,  1.0f,  1.0f,
			-1.0f, -1.0f,  1.0f, 

			-1.0f,  1.0f,  1.0f,
			-1.0f,  1.0f, -1.0f,
			-1.0f, -1.0f, -1.0f,
			-1.0f, -1.0f, -1.0f, 
			-1.0f, -1.0f,  1.0f,
			-1.0f,  1.0f,  1.0f, 

			1.0f,  1.0f,  1.0f, 
			1.0f,  1.0f, -1.0f, 
			1.0f, -1.0f, -1.0f, 
			1.0f, -1.0f, -1.0f, 
			1.0f, -1.0f,  1.0f,
			1.0f,  1.0f,  1.0f, 

			-1.0f, -1.0f, -1.0f, 
			1.0f, -1.0f, -1.0f, 
			1.0f, -1.0f,  1.0f, 
			1.0f, -1.0f,  1.0f,
			-1.0f, -1.0f,  1.0f,  
			-1.0f, -1.0f, -1.0f, 

			-1.0f,  1.0f, -1.0f, 
			1.0f,  1.0f, -1.0f, 
			1.0f,  1.0f,  1.0f, 
			1.0f,  1.0f,  1.0f,
			-1.0f,  1.0f,  1.0f, 
			-1.0f,  1.0f, -1.0f 
		};

		Skybox::Skybox()
		{
		}
		Skybox::~Skybox()
		{
		
		}

		void Skybox::Initialize( const std::array<Assets::Texture*, 6> & data)
		{
			for (auto i : data)
			{
				if (i == nullptr)
				{
					//Log Error?
					return;
				}
			}
			//TODO
			TextureDesc TexDesc;
			TexDesc.Name = "SkyBox_TextureCube";
			TexDesc.Type = RESOURCE_DIM_TEX_CUBE;
			TexDesc.Usage = USAGE_DEFAULT;
			TexDesc.BindFlags = BIND_SHADER_RESOURCE;
			TexDesc.Width = data.at(0)->GetWidth();
			TexDesc.Height = data.at(0)->GetHeight();
			TexDesc.Format = TEX_FORMAT_RGBA8_UNORM;
			TexDesc.ArraySize = 6;
			TexDesc.MipLevels = 1;

			RefCntAutoPtr<ITexture> TexCube;

			Graphics::GraphicsModule::Get().GetDevice()->CreateTexture(TexDesc, nullptr, &TexCube);

			for (unsigned int i = 0; i < data.size(); i++)
			{
				CopyTextureAttribs attrib;
				attrib.pSrcTexture = data[i]->GetTextureView()->GetTexture();
				attrib.DstSlice = i;
				attrib.pDstTexture = TexCube;
				Graphics::GraphicsModule::Get().GetContext()->CopyTexture(attrib);

			}

			if (TexCube != nullptr)
			{
				mTextureSRV = TexCube->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

				InitializePipeline();
				InitializeCube();
			}
		}

		void Skybox::Initialize(Assets::Texture* tex)
		{
			const auto& desc = Graphics::GraphicsModule::Get().GetSwapChain()->GetDesc();
			Initialize(tex, desc.ColorBufferFormat, desc.DepthBufferFormat);
		}

		bool Skybox::Initialize(Assets::Texture* texture, TEXTURE_FORMAT colorFormat, TEXTURE_FORMAT depthFormat)
		{
			Valid = false;
			if (!texture || !texture->GetTextureView() ||
				texture->GetTextureView()->GetTexture()->GetDesc().Type != RESOURCE_DIM_TEX_CUBE)
				return false;
			mTextureSRV = texture->GetTextureView();
			if (!InitializePipeline(colorFormat, depthFormat))
				return false;
			InitializeCube();
			Valid = mVBuffer != nullptr;
			return Valid;
		}
	

		void Skybox::Render()
		{
			if (Valid)
			{
				Graphics::GraphicsModule::Get().GetContext()->SetPipelineState(mPipeline);
				Graphics::GraphicsModule::Get().GetContext()->CommitShaderResources(mSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

				RenderCube();
			}
		}

		void Skybox::InitializePipeline()
		{
			const auto& desc = Graphics::GraphicsModule::Get().GetSwapChain()->GetDesc();
			InitializePipeline(desc.ColorBufferFormat, desc.DepthBufferFormat);
		}

		bool Skybox::InitializePipeline(TEXTURE_FORMAT colorFormat, TEXTURE_FORMAT depthFormat)
		{
			Valid = false;
			GraphicsPipelineStateCreateInfo PSOCreateInfo;
			PSOCreateInfo.PSODesc.Name = "SkyBox_PSO";
			PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
			PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = colorFormat;
			PSOCreateInfo.GraphicsPipeline.BlendDesc.RenderTargets[0].BlendEnable = false;
			PSOCreateInfo.GraphicsPipeline.DSVFormat = depthFormat;
			PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			PSOCreateInfo.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = true;
			PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
			PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;
			PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_LESS_EQUAL;
			PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = false;
			PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;

			//Create Shaders
			RefCntAutoPtr<IShader> VSShader;
			RefCntAutoPtr<IShader> PSShader;
			Graphics::GraphicsModule::Get().CreateShader(Platform::FileSystem::Get().LoadFileToString("@NuclearAssets@/Shaders/Background.vs.hlsl"), VSShader.RawDblPtr(), SHADER_TYPE_VERTEX);
			Graphics::GraphicsModule::Get().CreateShader(Platform::FileSystem::Get().LoadFileToString("@NuclearAssets@/Shaders/Background.Ps.hlsl"), PSShader.RawDblPtr(), SHADER_TYPE_PIXEL);
			if (!VSShader || !PSShader)
				return false;


			LayoutElement LayoutElems[] =
			{
				// Attribute 0 - vertex position
				LayoutElement{0, 0, 3, VT_FLOAT32, False,0},
			};

			PSOCreateInfo.pVS = VSShader;
			PSOCreateInfo.pPS = PSShader;
			PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
			PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElems);
			PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

			ShaderResourceVariableDesc Vars[] =
			{
				{SHADER_TYPE_PIXEL, "NE_Skybox", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
			};
			PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars;
			PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

			// Define static sampler for g_Texture. Static samplers should be used whenever possible
			SamplerDesc SamLinearClampDesc(FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
				TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP);

			SamLinearClampDesc.ComparisonFunc = COMPARISON_FUNC_ALWAYS;
			ImmutableSamplerDesc StaticSamplers[] =
			{
				{SHADER_TYPE_PIXEL, "NE_Skybox", SamLinearClampDesc}
			};
			PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers = StaticSamplers;
			PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(StaticSamplers);
			Graphics::GraphicsModule::Get().GetDevice()->CreateGraphicsPipelineState(PSOCreateInfo, &mPipeline);
			if (!mPipeline)
				return false;

			auto* camera = mPipeline->GetStaticVariableByName(SHADER_TYPE_VERTEX, "NEStatic_Camera");
			if (!camera)
				return false;
			camera->Set(RenderingModule::Get().GetCameraCB());


			mPipeline->CreateShaderResourceBinding(&mSRB, true);
			if (!mSRB)
				return false;
			auto* sky = mSRB->GetVariableByName(SHADER_TYPE_PIXEL, "NE_Skybox");
			if (!sky)
				return false;
			sky->Set(mTextureSRV);
			Valid = true;
			return true;
		}

		void Skybox::InitializeCube()
		{
			BufferDesc VertBuffDesc;
			VertBuffDesc.Usage = USAGE_IMMUTABLE;
			VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
			VertBuffDesc.Size = sizeof(skyboxVertices);

			BufferData VBData;
			VBData.pData = skyboxVertices;
			VBData.DataSize = sizeof(skyboxVertices);
			Graphics::GraphicsModule::Get().GetDevice()->CreateBuffer(VertBuffDesc, &VBData, &mVBuffer);
		}

		void Skybox::RenderCube()
		{
			Graphics::GraphicsModule::Get().GetContext()->SetVertexBuffers(0, 1, &mVBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);

			DrawAttribs DrawAttrs;
			DrawAttrs.NumVertices = 36;
			Graphics::GraphicsModule::Get().GetContext()->Draw(DrawAttrs);
		}

	}
}
