#include "Engine\Rendering\RenderPass\ShadowPass.h"
#include <Core\FileSystem.h>
#include <Engine\Graphics\Context.h>
#include <Core\Logger.h>
#include <Engine\Components/MeshComponent.h>
#include <Engine\Assets/Mesh.h>
#include <Diligent/Graphics/GraphicsTools/interface/MapHelper.hpp>
#include <Engine\Components\EntityInfoComponent.h>
#include <Engine\Assets\Scene.h>

namespace Nuclear
{
	namespace Rendering
	{
		struct NEStatic_LightInfo {
			Math::Matrix4 Model;
			Math::Matrix4 LightSpace;
		};

		ShadowPass::ShadowPass(const ShadowPassDesc& desc)
		{
			mDesc = desc;
		}

		void ShadowPass::Initialize()
		{
			InitPositionalShadowPassPSO();
			InitOmniDirShadowPassPSO();

			BufferDesc CBDesc;
			CBDesc.Name = "NEStatic_ShadowCasters";
			CBDesc.Size = (sizeof(Math::Matrix4) * mDesc.MAX_DIR_CASTERS) + (sizeof(Math::Matrix4) * mDesc.MAX_SPOT_CASTERS);
			CBDesc.Usage = USAGE_DYNAMIC;
			CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
			CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
			Graphics::Context::GetDevice()->CreateBuffer(CBDesc, nullptr, &pVSShadowCasterBuffer);
		}

		void ShadowPass::DirLightShadowDepthPass(Components::DirLightComponent& light, Assets::Scene* scene)
		{
			if (light.GetShadowType() == Components::LightShadowType::Simple_Shadows)
			{
				PositionalLightShadowDepthPass(light.GetShadowMap(), light.LightSpace, scene);
			}

			//CSM - WIP
		}

		void ShadowPass::SpotLightShadowDepthPass(Components::SpotLightComponent& spotlight, Assets::Scene* scene)
		{
			return PositionalLightShadowDepthPass(spotlight.GetShadowMap(), spotlight.LightSpace, scene);
		}

		void ShadowPass::PointLightShadowDepthPass(Components::PointLightComponent& pointlight, Assets::Scene* scene)
		{
			Graphics::Context::GetContext()->SetPipelineState(mOmniDirShadowPassPSO.RawPtr());

			Graphics::Context::GetContext()->SetRenderTargets(0, nullptr, pointlight.GetShadowMap()->GetRTV(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
			Graphics::Context::GetContext()->ClearDepthStencil(pointlight.GetShadowMap()->GetRTV(), CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

			Graphics::Context::GetContext()->CommitShaderResources(mOmniDirShadowPassSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

			auto view = scene->GetRegistry().view<Components::MeshComponent>();
			for (auto entity : view)
			{
				auto& MeshObject = view.get<Components::MeshComponent>(entity);
				if (MeshObject.mRender && MeshObject.mCastShadows)               //TODO Animation component
				{
					auto& EntityInfo = scene->GetRegistry().get<Components::EntityInfoComponent>(entity);
					EntityInfo.mTransform.Update();

					float near_plane = 1.0f;
					float far_plane = 100.0f;
					auto lightPos = pointlight.GetInternalPosition();

					//Update cbuffer NEStatic_PointShadowVS	{	matrix Model;	};
					{
						Diligent::MapHelper<Math::Matrix4> CBConstants(Graphics::Context::GetContext(), pOmniDirShadowVS_CB, MAP_WRITE, MAP_FLAG_DISCARD);
						*CBConstants = EntityInfo.mTransform.GetWorldMatrix();
					}
					//Update cbuffer NEStatic_PointShadowGS	{matrix ShadowMatrices[6];	};
					{

						glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), (float)1024 / (float)1024, near_plane, far_plane);

						std::array<glm::mat4, 6> shadowTransforms = {
						shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)),  //right
						shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)),  //left
						shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),    //up
						shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)),  //down
						shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f)),   //front
						shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))  //back
						};


						PVoid data;
						Graphics::Context::GetContext()->MapBuffer(pOmniDirShadowGS_CB, MAP_WRITE, MAP_FLAG_DISCARD, (PVoid&)data);
						data = memcpy(data, shadowTransforms.data(), sizeof(Math::Matrix4) * 6);
						Graphics::Context::GetContext()->UnmapBuffer(pOmniDirShadowGS_CB, MAP_WRITE);
					}
				//	Update cbuffer NEStatic_PointShadowPS	{    float3 gLightPos;	float gFarPlane;	};
				    {
						Diligent::MapHelper<Math::vec4> CBConstants(Graphics::Context::GetContext(), pOmniDirShadowPS_CB, MAP_WRITE, MAP_FLAG_DISCARD);
						*CBConstants = glm::vec4(lightPos, far_plane);
					}
					RenderMeshForDepthPass(MeshObject.mMesh);
				}
			}
		}

		ShadowPassDesc ShadowPass::GetDesc() const
		{
			return mDesc;
		}

		IBuffer* ShadowPass::GetShadowCastersCB()
		{
			return pVSShadowCasterBuffer;
		}
		void ShadowPass::PositionalLightShadowDepthPass(Graphics::ShadowMap* shadowmap, const Math::Matrix4 lightspace, Assets::Scene* scene)
		{
			Graphics::Context::GetContext()->SetPipelineState(mPositionalShadowMapDepthPSO.RawPtr());

			Graphics::Context::GetContext()->SetRenderTargets(0, nullptr, shadowmap->GetRTV(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
			Graphics::Context::GetContext()->ClearDepthStencil(shadowmap->GetRTV(), CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

			Graphics::Context::GetContext()->CommitShaderResources(mPositionalShadowMapDepthSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

			auto view = scene->GetRegistry().view<Components::MeshComponent>();
			for (auto entity : view)
			{
				auto& MeshObject = view.get<Components::MeshComponent>(entity);
				if (MeshObject.mRender && MeshObject.mCastShadows)               //TODO Animation component
				{
					auto& EntityInfo = scene->GetRegistry().get<Components::EntityInfoComponent>(entity);
					EntityInfo.mTransform.Update();

					NEStatic_LightInfo cbdata;
					cbdata.Model = EntityInfo.mTransform.GetWorldMatrix();
					cbdata.LightSpace = lightspace;
					{
						Diligent::MapHelper<NEStatic_LightInfo> CBConstants(Graphics::Context::GetContext(), pPositionalLightInfoCB, MAP_WRITE, MAP_FLAG_DISCARD);
						*CBConstants = cbdata;
					}

					RenderMeshForDepthPass(MeshObject.mMesh);
				}
			}
		}
		void ShadowPass::InitPositionalShadowPassPSO()
		{
			GraphicsPipelineStateCreateInfo PSOCreateInfo;

			PSOCreateInfo.PSODesc.Name = "PositionalShadowMapDepthPass PSO";
			PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 0;
			PSOCreateInfo.GraphicsPipeline.DSVFormat = TEX_FORMAT_D32_FLOAT;
			PSOCreateInfo.GraphicsPipeline.BlendDesc.RenderTargets[0].BlendEnable = false;
			PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			PSOCreateInfo.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = false;
			PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
			PSOCreateInfo.GraphicsPipeline.RasterizerDesc.FillMode = FILL_MODE_SOLID;
			PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;
			PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.StencilEnable = false;
			PSOCreateInfo.GraphicsPipeline.RasterizerDesc.DepthBias = 8500; //maybe as parameter
			PSOCreateInfo.GraphicsPipeline.RasterizerDesc.DepthBiasClamp = 0.0f;
			PSOCreateInfo.GraphicsPipeline.RasterizerDesc.SlopeScaledDepthBias = 1.0f;
			PSOCreateInfo.GraphicsPipeline.RasterizerDesc.DepthClipEnable = true;

			//Create Shaders
			RefCntAutoPtr<IShader> VSShader;
			RefCntAutoPtr<IShader> PSShader;
			std::vector<LayoutElement> LayoutElems;

			LayoutElems.push_back(LayoutElement(0, 0, 3, VT_FLOAT32, false));//POS
			LayoutElems.push_back(LayoutElement(1, 0, 2, VT_FLOAT32, false));  //UV
			LayoutElems.push_back(LayoutElement(2, 0, 3, VT_FLOAT32, false));  //NORMAL
			LayoutElems.push_back(LayoutElement(3, 0, 3, VT_FLOAT32, false));  //Tangents
			LayoutElems.push_back(LayoutElement(4, 0, 3, VT_FLOAT32, false));  //Bitangents
			LayoutElems.push_back(LayoutElement(5, 0, 4, VT_INT32, false));    //BONE ID
			LayoutElems.push_back(LayoutElement(6, 0, 4, VT_FLOAT32, false));  //WEIGHT

			auto source = Core::FileSystem::LoadFileToString("Assets/NuclearEngine/Shaders/PositionalShadowDepthPass.hlsl");

			//Create Vertex Shader
			{
				ShaderCreateInfo CreationAttribs;
				CreationAttribs.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
				CreationAttribs.UseCombinedTextureSamplers = true;
				CreationAttribs.Desc.ShaderType = SHADER_TYPE_VERTEX;
				CreationAttribs.EntryPoint = "PositionalShadowMapDepthVS";
				CreationAttribs.Desc.Name = "Positional_ShadowMapDepthVS";

				CreationAttribs.Source = source.c_str();

				Graphics::Context::GetDevice()->CreateShader(CreationAttribs, &VSShader);
			}

			//Create Pixel Shader
			{
				ShaderCreateInfo CreationAttribs;
				CreationAttribs.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
				CreationAttribs.UseCombinedTextureSamplers = true;
				CreationAttribs.Desc.ShaderType = SHADER_TYPE_PIXEL;
				CreationAttribs.EntryPoint = "PositionalShadowMapDepthPS";
				CreationAttribs.Desc.Name = "Positional_ShadowMapDepthPS";
				CreationAttribs.Source = source.c_str();


				Graphics::Context::GetDevice()->CreateShader(CreationAttribs, &PSShader);
			}

			PSOCreateInfo.pVS = VSShader;
			PSOCreateInfo.pPS = PSShader;
			PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems.data();
			PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = static_cast<Uint32>(LayoutElems.size());
			auto Vars = Graphics::GraphicsEngine::GetShaderManager()->ReflectShaderVariables(VSShader, PSShader);
			Graphics::GraphicsEngine::GetShaderManager()->ProcessAndCreatePipeline(&mPositionalShadowMapDepthPSO, PSOCreateInfo, Vars, true);

			BufferDesc CBDesc;
			CBDesc.Name = "PositionalLightInfo_CB";
			CBDesc.Size = sizeof(NEStatic_LightInfo);
			CBDesc.Usage = USAGE_DYNAMIC;
			CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
			CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
			Graphics::Context::GetDevice()->CreateBuffer(CBDesc, nullptr, &pPositionalLightInfoCB);

			mPositionalShadowMapDepthPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "NEStatic_LightInfo")->Set(pPositionalLightInfoCB);

			mPositionalShadowMapDepthPSO->CreateShaderResourceBinding(mPositionalShadowMapDepthSRB.RawDblPtr(), true);
		}
		void ShadowPass::InitPositionalShadowMapTexture()
		{
			TextureDesc ShadowMapDesc;
			ShadowMapDesc.Name = "PosShadowMap";
			ShadowMapDesc.Width = mDesc.mPositionalShadowMapInfo.mResolution;
			ShadowMapDesc.Height = mDesc.mPositionalShadowMapInfo.mResolution;
			ShadowMapDesc.MipLevels = 1;
			ShadowMapDesc.SampleCount = 1;
			ShadowMapDesc.Format = TEX_FORMAT_R32_TYPELESS;
			ShadowMapDesc.Usage = USAGE_DEFAULT;
			ShadowMapDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_DEPTH_STENCIL;
			ShadowMapDesc.Type = RESOURCE_DIM_TEX_2D_ARRAY;
			ShadowMapDesc.ArraySize = mDesc.MAX_POS_CASTERS;

			Graphics::Context::GetDevice()->CreateTexture(ShadowMapDesc, nullptr, &pPosShadowMap);

			{
				TextureViewDesc SRVDesc{ "PosShadowMap_SRV", TEXTURE_VIEW_SHADER_RESOURCE, RESOURCE_DIM_TEX_2D_ARRAY };
				SRVDesc.Format = TEX_FORMAT_R32_FLOAT;
				pPosShadowMap->CreateView(SRVDesc, &pPosShadowMapSRV);
			}
		}
		void ShadowPass::InitOmniDirShadowPassPSO()
		{
			GraphicsPipelineStateCreateInfo PSOCreateInfo;

			PSOCreateInfo.PSODesc.Name = "OmniDirShadowMapDepthPass PSO";
			PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 0;
			PSOCreateInfo.GraphicsPipeline.DSVFormat = TEX_FORMAT_D32_FLOAT;
			PSOCreateInfo.GraphicsPipeline.BlendDesc.RenderTargets[0].BlendEnable = false;
			PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			PSOCreateInfo.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = false;
			PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
			PSOCreateInfo.GraphicsPipeline.RasterizerDesc.FillMode = FILL_MODE_SOLID;
			PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;
			PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.StencilEnable = false;
			//PSOCreateInfo.GraphicsPipeline.RasterizerDesc.DepthBias = 8500; //maybe as parameter
			//PSOCreateInfo.GraphicsPipeline.RasterizerDesc.DepthBiasClamp = 0.0f;
			//PSOCreateInfo.GraphicsPipeline.RasterizerDesc.SlopeScaledDepthBias = 1.0f;
			//PSOCreateInfo.GraphicsPipeline.RasterizerDesc.DepthClipEnable = true;

			//Create Shaders
			RefCntAutoPtr<IShader> VSShader;
			RefCntAutoPtr<IShader> GSShader;
			RefCntAutoPtr<IShader> PSShader;
			std::vector<LayoutElement> LayoutElems;

			LayoutElems.push_back(LayoutElement(0, 0, 3, VT_FLOAT32, false));//POS
			LayoutElems.push_back(LayoutElement(1, 0, 2, VT_FLOAT32, false));  //UV
			LayoutElems.push_back(LayoutElement(2, 0, 3, VT_FLOAT32, false));  //NORMAL
			LayoutElems.push_back(LayoutElement(3, 0, 3, VT_FLOAT32, false));  //Tangents
			LayoutElems.push_back(LayoutElement(4, 0, 3, VT_FLOAT32, false));  //Bitangents
			LayoutElems.push_back(LayoutElement(5, 0, 4, VT_INT32, false));    //BONE ID
			LayoutElems.push_back(LayoutElement(6, 0, 4, VT_FLOAT32, false));  //WEIGHT

			auto source = Core::FileSystem::LoadFileToString("Assets/NuclearEngine/Shaders/OmniDirShadowDepthPass.hlsl");

			//Create Vertex Shader
			{
				ShaderCreateInfo CreationAttribs;
				CreationAttribs.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
				CreationAttribs.UseCombinedTextureSamplers = true;
				CreationAttribs.Desc.ShaderType = SHADER_TYPE_VERTEX;
				CreationAttribs.EntryPoint = "OmniDirShadowMapDepthVS";
				CreationAttribs.Desc.Name = "OmniDir_ShadowMapDepthVS";

				CreationAttribs.Source = source.c_str();

				Graphics::Context::GetDevice()->CreateShader(CreationAttribs, &VSShader);
			}

			//Create Geometry Shader
			{
				ShaderCreateInfo CreationAttribs;
				CreationAttribs.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
				CreationAttribs.UseCombinedTextureSamplers = true;
				CreationAttribs.Desc.ShaderType = SHADER_TYPE_GEOMETRY;
				CreationAttribs.EntryPoint = "OmniDirShadowMapDepthGS";
				CreationAttribs.Desc.Name = "OmniDir_ShadowMapDepthGS";

				CreationAttribs.Source = source.c_str();

				Graphics::Context::GetDevice()->CreateShader(CreationAttribs, &GSShader);
			}

			//Create Pixel Shader
			{
				ShaderCreateInfo CreationAttribs;
				CreationAttribs.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
				CreationAttribs.UseCombinedTextureSamplers = true;
				CreationAttribs.Desc.ShaderType = SHADER_TYPE_PIXEL;
				CreationAttribs.EntryPoint = "OmniDirShadowMapDepthPS";
				CreationAttribs.Desc.Name = "OmniDir_ShadowMapDepthPS";
				CreationAttribs.Source = source.c_str();


				Graphics::Context::GetDevice()->CreateShader(CreationAttribs, &PSShader);
			}

			PSOCreateInfo.pVS = VSShader;
			PSOCreateInfo.pGS = GSShader;
			PSOCreateInfo.pPS = PSShader;
			PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems.data();
			PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = static_cast<Uint32>(LayoutElems.size());
			auto Vars = Graphics::GraphicsEngine::GetShaderManager()->ReflectShaderVariables(VSShader, PSShader);
			Graphics::GraphicsEngine::GetShaderManager()->ProcessAndCreatePipeline(&mOmniDirShadowPassPSO, PSOCreateInfo, Vars, true);

			{
				BufferDesc CBDesc;
				CBDesc.Name = "NEStatic_OmniDirShadowVS";
				CBDesc.Size = sizeof(Math::Matrix4);
				CBDesc.Usage = USAGE_DYNAMIC;
				CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
				CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
				Graphics::Context::GetDevice()->CreateBuffer(CBDesc, nullptr, &pOmniDirShadowVS_CB);

				CBDesc.Name = "NEStatic_OmniDirShadowGS";
				CBDesc.Size = sizeof(Math::Matrix4) * 6;
				Graphics::Context::GetDevice()->CreateBuffer(CBDesc, nullptr, &pOmniDirShadowGS_CB);

				CBDesc.Name = "NEStatic_OmniDirShadowPS";
				CBDesc.Size = sizeof(Math::Vector4);

				Graphics::Context::GetDevice()->CreateBuffer(CBDesc, nullptr, &pOmniDirShadowPS_CB);
			}
			mOmniDirShadowPassPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "NEStatic_OmniDirShadowVS")->Set(pOmniDirShadowVS_CB);
			mOmniDirShadowPassPSO->GetStaticVariableByName(SHADER_TYPE_GEOMETRY, "NEStatic_OmniDirShadowGS")->Set(pOmniDirShadowGS_CB);
			mOmniDirShadowPassPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "NEStatic_OmniDirShadowPS")->Set(pOmniDirShadowPS_CB);

			mOmniDirShadowPassPSO->CreateShaderResourceBinding(mOmniDirShadowPassSRB.RawDblPtr(), true);
		}
		void ShadowPass::RenderMeshForDepthPass(Assets::Mesh* mesh)
		{
			Uint64 offset = 0;

			for (size_t i = 0; i < mesh->mSubMeshes.size(); i++)
			{

				Graphics::Context::GetContext()->SetIndexBuffer(mesh->mSubMeshes.at(i).mIB, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
				Graphics::Context::GetContext()->SetVertexBuffers(0, 1, &mesh->mSubMeshes.at(i).mVB, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);

				DrawIndexedAttribs  DrawAttrs;
				DrawAttrs.IndexType = VT_UINT32;
				DrawAttrs.NumIndices = mesh->mSubMeshes.at(i).mIndicesCount;
				Graphics::Context::GetContext()->DrawIndexed(DrawAttrs);

			}
		}
	}
}