#include <Systems\RenderSystem.h>
#include <Graphics/GraphicsModule.h>
#include <Utilities/Logger.h>
#include <Components/EntityInfoComponent.h>
#include <Components\CameraComponent.h>
#include <Components\MeshComponent.h>
#include <Graphics/GraphicsModule.h>
#include <Assets\AssetManager.h>
#include <Core\Scene.h>
#include <Diligent/Graphics/GraphicsTools/interface/MapHelper.hpp>
#include <Assets\Material.h>
#include <Platform\FileSystem.h>
#include <Rendering\RenderPasses\DefferedPass.h>
#include <Rendering\RenderingModule.h>
#include <Assets\Shader.h>
#include <Rendering\ImageBasedLighting.h>
#include <Profiling/Profiler.h>
#include <chrono>
#include <thread>
namespace Nuclear
{
	namespace Systems
	{
		RenderSystem::RenderSystem()
		{
			mRenderData.pRenderer = this;
		}
		RenderSystem::~RenderSystem()
		{
		}

		bool RenderSystem::NeedsBaking()
		{
			if (mStatus.BakeLighting || mStatus.BakePipelines)
			{
				return true;
			}

			return false;
		}

		void AddToDefinesIfNotZero(std::set<std::string>& defines, const std::string& name, Uint32 value)
		{
			if (value > 0)
			{
				defines.insert(name + std::to_string(value));
			}
		}
	
		void RenderSystem::Bake(const RenderSystemBakingDesc& desc)
		{								
			BakeLights();
			mRenderToScreen = desc.mRenderToScreen;
			Graphics::ShaderPipelineBakingDesc bakedesc;
			bakedesc.mRTWidth = desc.RTWidth;
			bakedesc.mRTHeight = desc.RTHeight;

			Graphics::Rendering3DShaderBakingDesc rendering3d_bake_desc;
			rendering3d_bake_desc.DirLights = static_cast<Uint32>(GetDirLightsNum());
			rendering3d_bake_desc.SpotLights = static_cast<Uint32>(GetSpotLightsNum());
			rendering3d_bake_desc.PointLights = static_cast<Uint32>(GetPointLightsNum());
			rendering3d_bake_desc.pShadowPass = GetRenderPass<Rendering::ShadowPass>();
			rendering3d_bake_desc.pIBLContext = pIBLContext;

			if (GetRenderPass<Rendering::DefferedPass>())
				rendering3d_bake_desc.mRenderSystemHasDefferedPass = true;
			
			if (GetRenderPass<Rendering::ShadowPass>())
				rendering3d_bake_desc.mRenderSystemHasShadowPass = true;

			bakedesc.pRendering3DBakingDesc = &rendering3d_bake_desc;
			bakedesc.pVariantsFactory = &Graphics::GraphicsModule::Get().GetDefaultShaderPipelineVariantFactory();

			bakedesc.mStaticVariablesBindings.push_back({ Diligent::SHADER_TYPE_VERTEX, "NEStatic_Camera", Rendering::RenderingModule::Get().GetCameraCB() });
			bakedesc.mStaticVariablesBindings.push_back({ Diligent::SHADER_TYPE_VERTEX, "NEStatic_Animation", Rendering::RenderingModule::Get().GetAnimationCB() });
			bakedesc.mStaticVariablesBindings.push_back({ Diligent::SHADER_TYPE_PIXEL, "NEStatic_Lights", GetLightCB() });

			AddToDefinesIfNotZero(bakedesc.mDefines, "NE_DIR_LIGHTS_NUM ", rendering3d_bake_desc.DirLights);
			AddToDefinesIfNotZero(bakedesc.mDefines, "NE_SPOT_LIGHTS_NUM ", rendering3d_bake_desc.SpotLights);
			AddToDefinesIfNotZero(bakedesc.mDefines, "NE_POINT_LIGHTS_NUM ", rendering3d_bake_desc.PointLights);

			if (rendering3d_bake_desc.pShadowPass)
			{
				AddToDefinesIfNotZero(bakedesc.mDefines, "NE_MAX_DIR_CASTERS ", rendering3d_bake_desc.pShadowPass->GetBakingDesc().MAX_DIR_CASTERS);
				AddToDefinesIfNotZero(bakedesc.mDefines, "NE_MAX_SPOT_CASTERS ", rendering3d_bake_desc.pShadowPass->GetBakingDesc().MAX_SPOT_CASTERS);
				AddToDefinesIfNotZero(bakedesc.mDefines, "NE_MAX_OMNIDIR_CASTERS ", rendering3d_bake_desc.pShadowPass->GetBakingDesc().MAX_OMNIDIR_CASTERS);
			}

			if (pIBLContext)
			{
				bakedesc.mPostVariantReflectionCallback = ([this](Graphics::ShaderVariantReflection& reflection) {
						for (auto& i : reflection.mIBLTexturesInfo)
						{
							if (i.mTex.mUsageType == Assets::TextureUsageType::IrradianceMap)
							{
								i.mTex.pTexture = &pIBLContext->GetEnvironmentCapture()->mIrradiance;
							}
							else if (i.mTex.mUsageType == Assets::TextureUsageType::PreFilterMap)
							{
								i.mTex.pTexture = &pIBLContext->GetEnvironmentCapture()->mPrefiltered;
							}
							else if (i.mTex.mUsageType == Assets::TextureUsageType::BRDF_LUT)
							{
								i.mTex.pTexture = pIBLContext->GetBRDF_LUT();
							}
						}
					});
			
			}
			for (auto i : mRegisteredShaders)
			{
				if (i)
				{					
					if (i->GetState() == Assets::IAsset::State::Queued)
					{
						bool condition = true;
						for (; condition;)
						{
							//wait for half a seond
							std::this_thread::sleep_for(std::chrono::milliseconds(500));

							if (i->GetState() == Assets::IAsset::State::Loaded)
							{
								condition = false;
							}
						}
					}
					i->GetShaderPipeline().Bake(bakedesc);
				}
			}

		}
		void RenderSystem::ResizeRTs(Uint32 RTWidth, Uint32 RTHeight)
		{
			Math::Vector2ui newsize(RTWidth, RTHeight);

			for (auto pass : mRenderPasses)
			{
				pass->ResizeRTs(RTWidth, RTHeight);
			}

			for (auto i : mRegisteredShaders)
			{
				if (i)
				{
					if (i->GetShaderPipeline().GetGBuffer()->GetDimensions() != newsize)
					{
						i->GetShaderPipeline().GetGBuffer()->Resize(newsize);
					}
				}
			}
		}

		void RenderSystem::AddRenderPass(Rendering::RenderPass* pass)
		{
			mRenderPasses.push_back(pass);
		}		

		void RenderSystem::SetIBLContext(Rendering::ImageBasedLighting* IBLContext)
		{
			pIBLContext = IBLContext;
		}

		void RenderSystem::RegisterShader(Assets::Shader* shader)
		{
			mRegisteredShaders.push_back(shader);
		}

		void RenderSystem::Update(ECS::TimeDelta dt)
		{
			//////////////////////////////////////////////////////////////////////////////////////////////
			//Step 1: Update Entites
			//////////////////////////////////////////////////////////////////////////////////////////////
			UpdateLights();
			bool hasdefpasss = false, hasshadowpass = false;
			if (GetRenderPass<Rendering::DefferedPass>())
			{
				hasdefpasss = true;
			}

			auto shadowpass = GetRenderPass<Rendering::ShadowPass>();
			if (shadowpass)
			{
				mRenderData.pDirPosShadowMapSRV = shadowpass->GetDirPosShadowMapSRV();
				mRenderData.pSpotPosShadowMapSRV = shadowpass->GetSpotShadowMapSRV();
				mRenderData.pOmniDirShadowMapSRV = shadowpass->GetOmniDirShadowMapSRV();
				hasshadowpass = true;
			}

			auto meshview = Core::Scene::Get().GetRegistry().view<Components::MeshComponent>();
			for (auto entity : meshview)
			{
				auto& mesh = meshview.get<Components::MeshComponent>(entity);
				mesh.SetRenderSystemFlags(hasdefpasss, hasshadowpass);
				mesh.Update();
			}

			//sort accroding to shadingmodels (aka pipelines) & update their renderqueues
			Core::Scene::Get().GetRegistry().sort<Nuclear::Components::MeshComponent>([](const auto& lhs, const auto& rhs)
				{
					return lhs.GetRenderQueue() < rhs.GetRenderQueue();
				});

			mRenderData.mMeshView = Core::Scene::Get().GetRegistry().view<Components::MeshComponent>();

			//////////////////////////////////////////////////////////////////////////////////////////////
			//Step 2: Render scene from each camera pov
			//////////////////////////////////////////////////////////////////////////////////////////////
			auto CameraView = Core::Scene::Get().GetRegistry().view<Components::CameraComponent>();
			for (auto entity : CameraView)
			{
				auto& camera = CameraView.get<Components::CameraComponent>(entity);
				if (camera.GetIsActive())
				{
					UpdateLightsBuffer(Math::Vector4(camera.GetPosition(), 1.0f));

					////////////////////////////////////
					//Step 2.1: Build FrameRenderData
					////////////////////////////////////
					mRenderData.pCamera = &camera;
					mRenderData.mUsedDefferedPipelines.clear();

					////////////////////////////////////
					//Step 2.2: Clear main RTVs
					////////////////////////////////////
					Graphics::GraphicsModule::Get().GetContext()->SetRenderTargets(1, camera.GetColorRT().GetRTVDblPtr(), camera.GetDepthRT().GetRTV(), Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
					Graphics::GraphicsModule::Get().GetContext()->ClearRenderTarget(camera.GetColorRT().GetRTV(), (float*)&camera.mRTClearColor, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
					Graphics::GraphicsModule::Get().GetContext()->ClearDepthStencil(camera.GetDepthRT().GetRTV(), Diligent::CLEAR_DEPTH_FLAG, 1.0f, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
					{
					//	PROFILE_GPU(test);
						////////////////////////////////////
						//Step 2.3: Update RenderPasses
						////////////////////////////////////
						for (auto pass : mRenderPasses)
						{
							pass->Update(&mRenderData);
						}
				//		NUCLEAR_INFO("Test {0}", test.End());
					}
				}
			}		


			if (mRenderToScreen)
			{
				//Step 3: Copy Main camera RT to RenderingModule RTs.
				Diligent::CopyTextureAttribs attrib;
				attrib.pSrcTexture = Core::Scene::Get().GetMainCamera()->GetColorRT().GetSRV()->GetTexture();
				attrib.pDstTexture = Rendering::RenderingModule::Get().GetFinalRT().GetSRV()->GetTexture();
				Graphics::GraphicsModule::Get().GetContext()->CopyTexture(attrib);


				//---------TODO: Maybe copy main camera depth rt?--------------

				//Step 4: Render to screen
				Rendering::RenderingModule::Get().RenderFinalRT();
			}
		}

		//Shader Structs
		namespace ShaderReference
		{
			struct NEAPI Shader_SpotLight_Struct
			{
				Math::Vector4 Position;
				Math::Vector4 Direction;
				Math::Vector4 Intensity_Attenuation;
				Math::Vector4 InnerCutOf_OuterCutoff;
				Math::Vector4 Color;
			};

			struct NEAPI Shader_PointLight_Struct
			{
				Math::Vector4 Position;
				Math::Vector4 Intensity_Attenuation;
				Math::Vector4 Color_FarPlane;
			};

			struct NEAPI Shader_DirLight_Struct
			{
				Math::Vector4 Direction;
				Math::Vector4 Color_Intensity;

			};
		}

		void RenderSystem::BakeLightsBuffer()
		{
			if (HasbeenBakedBefore)
			{
				if (!LightRequiresBaking())
				{
					NUCLEAR_WARN("[LightingSystem] No need for baking the sub system!");
				}
			}

			HasbeenBakedBefore = true;

			NE_Light_CB_Size = sizeof(Math::Vector4);
			NUM_OF_LIGHT_VECS = 1;
			Baked_DirLights_Size = mRenderData.DirLights.size();
			Baked_PointLights_Size = mRenderData.PointLights.size();
			Baked_SpotLights_Size = mRenderData.SpotLights.size();

			if (mRenderData.DirLights.size() > 0)
			{
				NE_Light_CB_Size = NE_Light_CB_Size + (mRenderData.DirLights.size() * sizeof(ShaderReference::Shader_DirLight_Struct));
				NUM_OF_LIGHT_VECS = NUM_OF_LIGHT_VECS + (mRenderData.DirLights.size() * 2);
			}
			if (mRenderData.PointLights.size() > 0)
			{
				NE_Light_CB_Size = NE_Light_CB_Size + (mRenderData.PointLights.size() * sizeof(ShaderReference::Shader_PointLight_Struct));
				NUM_OF_LIGHT_VECS = NUM_OF_LIGHT_VECS + (mRenderData.PointLights.size() * 3);
			}
			if (mRenderData.SpotLights.size() > 0)
			{
				NE_Light_CB_Size = NE_Light_CB_Size + (mRenderData.SpotLights.size() * sizeof(ShaderReference::Shader_SpotLight_Struct));
				NUM_OF_LIGHT_VECS = NUM_OF_LIGHT_VECS + (mRenderData.SpotLights.size() * 5);
			}

			if (mPSLightCB.RawPtr() != nullptr)
			{
				mPSLightCB.Release();
			}
			Diligent::BufferDesc CBDesc;
			CBDesc.Name = "LightCB";
			CBDesc.Size = static_cast<Uint32>(NE_Light_CB_Size);
			CBDesc.Usage = Diligent::USAGE_DYNAMIC;
			CBDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
			CBDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
			Diligent::BufferData DATA;
			Graphics::GraphicsModule::Get().GetDevice()->CreateBuffer(CBDesc, &DATA, mPSLightCB.RawDblPtr());
		}

		void RenderSystem::BakeLights()
		{
			auto LightSourcesView = Core::Scene::Get().GetRegistry().view<Components::LightComponent>();

			std::vector<Components::LightComponent*> DirLights_noShadows;
			std::vector<Components::LightComponent*> PointLights_noShadows;
			std::vector<Components::LightComponent*> SpotLights_noShadows;

			//Shadows Enabled First
			for (auto entity : LightSourcesView)
			{
				auto& Light = LightSourcesView.get<Components::LightComponent>(entity);
				auto type = Light.GetType();

				if (type == Components::LightComponent::Type::Directional)
				{
					if (Light.mCastShadows)
						mRenderData.DirLights.push_back(&Light);
					else
						DirLights_noShadows.push_back(&Light);
				}
				else if (type == Components::LightComponent::Type::Point)
				{
					if (Light.mCastShadows)
						mRenderData.PointLights.push_back(&Light);
					else
						PointLights_noShadows.push_back(&Light);
				}			
				else if (type == Components::LightComponent::Type::Spot)
				{
					if (Light.mCastShadows)
						mRenderData.SpotLights.push_back(&Light);
					else
						SpotLights_noShadows.push_back(&Light);
				}
				else {
					assert(0);
				}
			}
			
			//append the non shadowed vectors
			mRenderData.DirLights.insert(mRenderData.DirLights.end(), DirLights_noShadows.begin(), DirLights_noShadows.end());
			mRenderData.SpotLights.insert(mRenderData.SpotLights.end(), SpotLights_noShadows.begin(), SpotLights_noShadows.end());
			mRenderData.PointLights.insert(mRenderData.PointLights.end(), PointLights_noShadows.begin(), PointLights_noShadows.end());

			BakeLightsBuffer();
		}

		void RenderSystem::UpdateLights()
		{
			auto LightView = Core::Scene::Get().GetRegistry().view<Components::LightComponent>();
			for (auto entity : LightView)
			{
				auto& Light = LightView.get<Components::LightComponent>(entity);
				auto& Einfo = Core::Scene::Get().GetRegistry().get<Components::EntityInfoComponent>(entity);
				Light.SetInternalPosition(Einfo.mTransform.GetLocalPosition());
			}		
		}
		void RenderSystem::UpdateLightsBuffer(const Math::Vector4& CameraPos)
		{
			std::vector<Math::Vector4> LightsBuffer;
			LightsBuffer.reserve(NUM_OF_LIGHT_VECS + 1);

			LightsBuffer.push_back(CameraPos);

			for (size_t i = 0; i < mRenderData.DirLights.size(); i++)
			{
				auto& data = mRenderData.DirLights[i]->GetInternalData();
				LightsBuffer.push_back(data.Direction);
				LightsBuffer.push_back(data.Color_Intensity);
			}
			for (size_t i = 0; i < mRenderData.PointLights.size(); i++)
			{
				auto& data = mRenderData.PointLights[i]->GetInternalData();

				LightsBuffer.push_back(data.Position);
				Math::Vector4 Intensity_Attenuation(data.Color_Intensity.w, data.Attenuation_FarPlane.x, data.Attenuation_FarPlane.y, data.Attenuation_FarPlane.z);
				LightsBuffer.push_back(Intensity_Attenuation);
				Math::Vector4 Color_FarPlane(Math::Vector3(data.Color_Intensity) , data.Attenuation_FarPlane.w);
				LightsBuffer.push_back(Color_FarPlane);

			}
			for (size_t i = 0; i < mRenderData.SpotLights.size(); i++)
			{
				auto& data = mRenderData.SpotLights[i]->GetInternalData();

				LightsBuffer.push_back(data.Position);
				LightsBuffer.push_back(data.Direction);
				Math::Vector4 Intensity_Attenuation(data.Color_Intensity.w, data.Attenuation_FarPlane.x, data.Attenuation_FarPlane.y, data.Attenuation_FarPlane.z);
				LightsBuffer.push_back(Intensity_Attenuation);
				LightsBuffer.push_back(data.InnerCutOf_OuterCutoff);
				Math::Vector4 Color(Math::Vector3(data.Color_Intensity), 0.0f);
				LightsBuffer.push_back(Color);
			}

			PVoid data = NULL;
			Graphics::GraphicsModule::Get().GetContext()->MapBuffer(mPSLightCB, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, (PVoid&)data);
			data = memcpy(data, LightsBuffer.data(), NE_Light_CB_Size);
			Graphics::GraphicsModule::Get().GetContext()->UnmapBuffer(mPSLightCB, Diligent::MAP_WRITE);
		}
		bool RenderSystem::LightRequiresBaking()
		{
			if (Baked_DirLights_Size == mRenderData.DirLights.size()
				&& Baked_PointLights_Size == mRenderData.PointLights.size()
				&& Baked_SpotLights_Size == mRenderData.SpotLights.size())
				return true;

			return false;
		}
		Diligent::IBuffer* RenderSystem::GetLightCB()
		{
			return mPSLightCB.RawPtr();
		}
		size_t RenderSystem::GetDirLightsNum()
		{
			return mRenderData.DirLights.size();
		}
		size_t RenderSystem::GetPointLightsNum()
		{
			return mRenderData.PointLights.size();
		}
		size_t RenderSystem::GetSpotLightsNum()
		{
			return mRenderData.SpotLights.size();
		}
	}
}
