#pragma once
#include "SampleBase.h"

//Current TODO:
//Shadow test
class Playground : public SampleBase
{
	Assets::Material* RustedIron;
	Assets::Material* Plastic;

	Assets::Font* ArialFont;

	Assets::Shader* PBR;

	/// Borrowed controller audio source; valid until the playground scene is cleared.
	Components::AudioSourceComponent* testcmp = nullptr;

	Rendering::GeometryPass GeoPass;
	//Rendering::PostProcessingPass PostFXPass;
	//Rendering::DefferedPass DefferedPass;
	//Rendering::ShadowPass ShadowPass;
public:
	Playground()
	{
	}
	/// Imports optional font/audio content and creates a scene-owned playback source when audio is ready.
	void SetupAssets()
	{

		//GetAssetManager().LoadFolder("@Assets@/Textures/PBR/RustedIron/");
		//GetAssetManager().LoadFolder("@Assets@/Textures/PBR/Plastic/");

		//Initialize Materials
	//	RustedIron = GetAssetManager().Load<Assets::Material>("@Assets@/Materials/PBR/RustedIron.NEMaterial");
		//Plastic = GetAssetManager().Load<Assets::Material>("@Assets@/Materials/PBR/Plastic.NEMaterial");

		Assets::FontImportingDesc fdesc;
		ArialFont = GetAssetManager().Import<Assets::Font> ("@CommonAssets@/Fonts/arial.ttf", fdesc);

		auto* audio = GetAssetManager().Import<Assets::AudioClip>("@Assets@/AudioClips/str3.mp3");
		if (audio && audio->GetState() == Assets::IAsset::State::Loaded && audio->GetBufferID() != 0)
		{
			GetScene().GetSystemManager().Add<Systems::AudioSystem>();
			EController.AddComponent<Components::AudioListenerComponent>();
			testcmp = &EController.AddComponent<Components::AudioSourceComponent>(audio);
			testcmp->Play();
		}
	}
	void SetupEntities()
	{
		//Create Entities
		auto EDirLight = GetScene().CreateEntity("DirLight");
		auto& dircomp = EDirLight.AddComponent<Components::LightComponent>(Components::LightComponent::Type::Directional);
		dircomp.SetDirection(Math::Vector3(-0.2f, -1.0f, -0.3f));

		auto ELights = GetScene().CreateEntity("PointLight1");
		auto& lightcomp = ELights.AddComponent<Components::LightComponent>(Components::LightComponent::Type::Point);
		lightcomp.mCastShadows = true;

		auto ELights2 = GetScene().CreateEntity("PointLight2");
		auto& light2comp = ELights2.AddComponent<Components::LightComponent>(Components::LightComponent::Type::Point);
		light2comp.mCastShadows = true;

		//Assign Components
		EController.AddComponent<Components::LightComponent>(Components::LightComponent::Type::Spot);

		ELights.GetComponent<Components::EntityInfoComponent>().mTransform.SetPosition(Math::Vector3(-2.0f, 4.0f, -1.0f));
	}


	void InitRenderer()
	{

		//PBR = GetAssetManager().Import<Assets::Shader>("@NuclearAssets@/Shaders/PBR/PBR.NuclearShader");
		//Renderer->RegisterShader(PBR);

	//	Renderer->AddRenderPass(&ShadowPass);
		//Renderer->AddRenderPass(&GeoPass);
		//Renderer->AddRenderPass(&DefferedPass);
	//	Renderer->AddRenderPass(&PostFXPass);

		Systems::RenderSystemBakingDesc bakedesc;
		bakedesc.RTWidth = _Width_;
		bakedesc.RTHeight = _Height_;
		Renderer->Bake(bakedesc);
		//PostFXPass.Bake({ _Width_, _Height_,Rendering::RenderingModule::Get().GetFinalRT().GetDesc() });
	}

	void Load()
	{
		SampleBase::Load();
		Rendering::ShadowPassBakingDesc spdesc;

		spdesc.MAX_OMNIDIR_CASTERS =2;
		spdesc.MAX_SPOT_CASTERS = 0;
		//ShadowPass.Bake(spdesc);

		SetupEntities();

		InitRenderer();

		SetupAssets();

/*
		// cubes
		{
			ECS::Transform TSphere;
			TSphere.SetPosition(Math::Vector3(0.0f, 1.5f, 0.0));
		//	TSphere.SetScale(Math::Vector3(0.5f));
			GetScene().CreateBox(RustedIron, TSphere);
		}
		{		
			ECS::Transform TSphere;

			TSphere.SetPosition(Math::Vector3(2.0f, 0.0f, 1.0));
		//	TSphere.SetScale(Math::Vector3(0.5f));
			GetScene().CreateBox(RustedIron, TSphere);
		}
		{		
			ECS::Transform TSphere;

			TSphere.SetPosition(Math::Vector3(-1.0f, 0.0f, 2.0));
			TSphere.SetRotation(glm::normalize(glm::vec3(1.0, 0.0, 1.0)), glm::radians(60.0f));
	//		TSphere.SetScale(Math::Vector3(0.25));
			GetScene().CreateBox(RustedIron, TSphere);
		}
		GetScene().CreatePlane(Plastic);

		GetScene().GetMainCamera()->mRTClearColor = Graphics::Color(0.15f, 0.15f, 0.15f, 1.0f);*/
		//GetScene().GetMainCamera()->MovementSpeed = 15;
		//Renderer->VisualizePointLightsPositions = true;
		//EController.GetComponent<Components::SpotLightComponent>().mCastShadows = true;

		Platform::Input::Get().SetMouseInputMode(Platform::Input::MouseInputMode::Locked);
	}
	bool iskinematic = false;

	void Render(float dt) override
	{
		GetScene().Update(dt);
		static bool LockSpotlight = true;

		if (LockSpotlight)
		{
			EController.GetComponent<Components::EntityInfoComponent>().mTransform.SetPosition(GetScene().GetMainCamera()->GetPosition());
			EController.GetComponent<Components::LightComponent>().SetDirection(GetScene().GetMainCamera()->GetFrontView());
		}
		{
			using namespace Graphics;
			ImGui::Begin("PlayGround : Testing new features");

			ImGui::Text("Press M to enable mouse capturing, or Esc to disable mouse capturing");

			//ImGui::Texture(ArialFont->mTextureView, {256.f,256.f });

			ImGui::ColorEdit3("Camera ClearColor", (float*)&GetScene().GetMainCamera()->mRTClearColor);

			ImGui::Checkbox("ShowRegisteredRenderTargets", &DebugSystem->ShowRegisteredRenderTargets);
			ImGui::Checkbox("LockSpotlight", &LockSpotlight);
			ImGui::Checkbox("RenderLightSources", &DebugSystem->RenderLightSources);

			static float v = 1.0f, p = 1.0f;
			if (ImGui::SliderFloat("volume: ", &v, 0.0f, 1.0f) && testcmp)
			{
				testcmp->SetVolume(v);
			}
			
			if (ImGui::SliderFloat("pitch: ", &p, 0.5f, 2.0f) && testcmp)
			{
				testcmp->SetPitch(p);
			}

			/*if (ImGui::TreeNode("PostFX Effects"))
			{
				for (auto& it : PostFXPass.GetPipelineController().GetSwitches())
				{
					bool value = it.second.GetValue();
					ImGui::Checkbox(it.second.GetName().c_str(), &value);
					if (value != it.second.GetValue())
					{
						PostFXPass.SetPostProcessingEffect(it.second.GetID(), value);
					}
				}
				ImGui::TreePop();
			}*/

			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

			if (ImGui::Button("End Game"))
			{
				ImGui::End();
				return Core::Engine::Get().EndClient();
			}

			ImGui::End();
			EntityExplorer();
		}
	}

	/// Ends playback before clearing scene components and their borrowed asset pointers.
	void Shutdown() override
	{
		if (testcmp)
		{
			testcmp->Pause();
			testcmp = nullptr;
		}
		Platform::Input::Get().SetMouseInputMode(Platform::Input::MouseInputMode::Normal);
		SampleBase::Shutdown();
	}
};
