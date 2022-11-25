#pragma once
#include "Common.h"

class SponzaDemo : public Client
{
	std::shared_ptr<Systems::RenderSystem> Renderer;
	std::shared_ptr<Systems::CameraSystem> mCameraSystem;
	std::shared_ptr<Systems::DebugSystem> mDebugSystem;

	Assets::Mesh* SponzaAsset;
	Assets::MaterialData* SponzaMaterial;

	Assets::Material SponzaPBRMaterial;
	Assets::Material SponzaBlinnPhongMaterial;

	Graphics::Camera Camera;

	Rendering::Skybox Skybox;

	Assets::Shader* PBR;
	Assets::Shader* BlinnPhong;

	///////////////////////////////////////////////////
	Rendering::DefferedRenderingPath DefferedRP;

	Rendering::GeometryPass GeoPass;
	Rendering::PostProcessingPass PostFXPass;
	Rendering::DefferedPass DefferedPass;

	//Rendering::ShadowPass ShadowPass;

	//IBL Settings
	Rendering::ImageBasedLighting IBL;
	Rendering::PBRCapture EnvCapture;
	Graphics::Texture HDREnv;
	Assets::Image HDR_Cube;

	Assets::Scene Scene;

	ECS::Entity ESponza;

	ECS::Entity EController;
	ECS::Entity ELights;

	// positions of the point lights
	Math::Vector3 pointLightPositions[9] =
	{
		Math::Vector3(0.7f,  0.2f,  2.0f),
		Math::Vector3(2.3f, -3.3f, -4.0f),
		Math::Vector3(-4.0f,  2.0f, -12.0f),
		Math::Vector3(0.0f,  0.0f, -3.0f),
		Math::Vector3(4.0f,  3.0f, -2.0f),
		Math::Vector3(6.2f, 2.0f, 0.0f),
		Math::Vector3(6.2f, -2.0f, 0.0f),
		Math::Vector3(-6.2f, 2.0f, 0.0f),
		Math::Vector3(-6.2f, -2.0f, 0.0f)
	};
	float lastX = _Width_ / 2.0f;
	float lastY = _Height_ / 2.0f;
	bool firstMouse = true;
	bool isMouseDisabled = false;

public:
	SponzaDemo()
		: Camera(Math::Vector3(0.0f, 0.0f, 0.0f), Math::Vector3(0.0f, 1.0f, 0.0f), Graphics::YAW, Graphics::PITCH, Graphics::SPEED, Graphics::SENSITIVTY, Graphics::ZOOM)
	{
	}
	void SetupAssets()
	{
		Importers::MeshLoadingDesc ModelDesc;
		Assets::Animations* Placeholder;

		//Load Sponza Model
		std::tie(SponzaAsset, SponzaMaterial, Placeholder) = mAssetManager->Import("@CommonAssets@/Models/CrytekSponza/sponza.fbx", ModelDesc);

		SponzaPBRMaterial.Create(SponzaMaterial, PBR);
		SponzaBlinnPhongMaterial.Create(SponzaMaterial, BlinnPhong);

		ESponza.AddComponent<Components::MeshComponent>(SponzaAsset, &SponzaPBRMaterial);

	}
	void SetupEntities()
	{
		//Create Entities
		ECS::Transform TSponza;
		TSponza.SetScale(Math::Vector3(0.05f, 0.05f, 0.05f));
		ESponza = Scene.CreateEntity("Sponza", TSponza);
		ELights = Scene.CreateEntity("Lights");

		ELights.AddComponent<Components::DirLightComponent>();
		ELights.AddComponent<Components::PointLightComponent>().mCastShadows = true;


		ELights.GetComponent<Components::DirLightComponent>()->SetDirection(Math::Vector3(-0.2f, -1.0f, -0.3f));
		ELights.GetComponent<Components::DirLightComponent>()->SetColor(Graphics::Color(0.4f, 0.4f, 0.4f, 0.0f));
		ELights.GetComponent<Components::EntityInfoComponent>()->mTransform.SetPosition(pointLightPositions[0]);
		ELights.GetComponent<Components::PointLightComponent>()->SetColor(Graphics::Color(1.0f, 1.0f, 1.0f, 0.0f));
		ELights.GetComponent<Components::PointLightComponent>()->SetIntensity(10.f);

		//for (int i = 1; i < 9; i++)
		//{
		//	auto Light = Scene.CreateEntity("Light" + std::to_string(i));
		//	Light.AddComponent<Components::PointLightComponent>();
		//	Light.GetComponent<Components::EntityInfoComponent>()->mTransform.SetPosition(pointLightPositions[i]);
		//	Light.GetComponent<Components::PointLightComponent>()->SetColor(Graphics::Color(1.0f, 1.0f, 1.0f, 0.0f));
		//	Light.GetComponent<Components::PointLightComponent>()->SetIntensity(2.f);
		//}

	}

	void InitShaders()
	{
		Importers::ShaderLoadingDesc desc;
		desc.mType = Importers::ShaderType::_3DRendering;
		PBR = mAssetManager->Import("@NuclearAssets@/Shaders/PBR/PBR.NEShader", desc);
		BlinnPhong = mAssetManager->Import("@NuclearAssets@/Shaders/BlinnPhong.NEShader", desc);

		Renderer->SetIBLContext(&IBL);
		Renderer->RegisterShader(PBR);
		Renderer->RegisterShader(BlinnPhong);
	}

	void InitIBL()
	{
		//IBL
		Importers::ImageLoadingDesc DESC;
		DESC.mType = RESOURCE_DIM_TEX_2D;
		DESC.mUsage = USAGE_IMMUTABLE;
		DESC.mBindFlags = BIND_SHADER_RESOURCE;
		DESC.mMipLevels = 1;

		HDREnv = mAssetManager->Import("@CommonAssets@/Textures/HDR/newport_loft.hdr", DESC, Graphics::TextureUsageType::Unknown);

		Rendering::ImageBasedLightingDesc desc;
		IBL.Initialize(desc);

		HDR_Cube = IBL.EquirectangularToCubemap(&HDREnv);
		EnvCapture = IBL.PrecomputePBRCapture(&HDR_Cube);
		IBL.SetEnvironmentCapture(&EnvCapture);
	}

	void InitRenderer()
	{
		Renderer = Scene.GetSystemManager().Add<Systems::RenderSystem>();

		InitIBL();

		GeoPass.pRenderingPath = &DefferedRP;

		Renderer->AddRenderPass(&GeoPass);
		Renderer->AddRenderPass(&DefferedPass);
		Renderer->AddRenderPass(&PostFXPass);


		InitShaders();

		Renderer->Bake(_Width_, _Height_);
		PostFXPass.Bake({ _Width_, _Height_,Renderer->mRenderData.mFinalRT.GetDesc() });

		PostFXPass.SetPostProcessingEffect(Utilities::Hash("HDR"), false);
		PostFXPass.SetPostProcessingEffect(Utilities::Hash("GAMMACORRECTION"), false);
		PostFXPass.SetPostProcessingEffect(Utilities::Hash("BLOOM"), false);

		Skybox.Initialize(mCameraSystem->GetCameraCB(), &HDR_Cube);
		//Renderer->GetBackground().SetSkybox(&Skybox);
	}

	void Load()
	{
		mSceneManager->CreateScene(&Scene, true);

		mAssetManager->Initialize();

		EController = Scene.CreateEntity();
		EController.AddComponent<Components::SpotLightComponent>();
		EController.AddComponent<Components::CameraComponent>(&Camera);

		Camera.Initialize(Math::perspective(Math::radians(45.0f), Engine::GetInstance()->GetMainWindow()->GetAspectRatioF32(), 0.1f, 100.0f));

		mCameraSystem = Scene.GetSystemManager().Add<Systems::CameraSystem>(&Camera);
		SetupEntities();

		//Rendering::ShadowPassBakingDesc spdesc;

		//spdesc.MAX_OMNIDIR_CASTERS = 1;
		//spdesc.MAX_DIR_CASTERS = 0;
		//ShadowPass.Bake(spdesc);

		//Systems::LightingSystemDesc desc;
		//desc.ShadowPass = &ShadowPass;

		//mLightingSystem = Scene.GetSystemManager().Add<Systems::LightingSystem>(desc);
		//mLightingSystem->Bake();

		InitRenderer();
	//	Renderer->AddRenderPass(&ShadowPass);

		SetupAssets();

		//Renderer->GetBackground().SetSkybox(&Skybox);
		Engine::GetInstance()->GetMainWindow()->SetMouseInputMode(Core::Input::MouseInputMode::Virtual);
	}
	void OnMouseMovement(int xpos_a, int ypos_a) override
	{
		if (!isMouseDisabled)
		{
			float xpos = static_cast<float>(xpos_a);
			float ypos = static_cast<float>(ypos_a);

			if (firstMouse)
			{
				lastX = xpos;
				lastY = ypos;
				firstMouse = false;
			}

			float xoffset = xpos - lastX;
			float yoffset = lastY - ypos;

			lastX = xpos;
			lastY = ypos;

			Camera.ProcessEye(xoffset, yoffset);
		}
	}


	void OnWindowResize(int width, int height) override
	{
		Graphics::Context::GetSwapChain()->Resize(width, height);
		Camera.SetProjectionMatrix(Math::perspective(Math::radians(45.0f), Engine::GetInstance()->GetMainWindow()->GetAspectRatioF32(), 0.1f, 400.0f));
	//	Renderer->ResizeRenderTargets(width, height);
	}
	void Update(float deltatime) override
	{
		//Movement
		if (Engine::GetInstance()->GetMainWindow()->GetKeyStatus(Core::Input::KeyboardKey::KEY_W) == Core::Input::KeyboardKeyStatus::Pressed)
			Camera.ProcessMovement(Graphics::CAMERA_MOVEMENT_FORWARD, deltatime);
		if (Engine::GetInstance()->GetMainWindow()->GetKeyStatus(Core::Input::KeyboardKey::KEY_A) == Core::Input::KeyboardKeyStatus::Pressed)
			Camera.ProcessMovement(Graphics::CAMERA_MOVEMENT_LEFT, deltatime);
		if (Engine::GetInstance()->GetMainWindow()->GetKeyStatus(Core::Input::KeyboardKey::KEY_S) == Core::Input::KeyboardKeyStatus::Pressed)
			Camera.ProcessMovement(Graphics::CAMERA_MOVEMENT_BACKWARD, deltatime);
		if (Engine::GetInstance()->GetMainWindow()->GetKeyStatus(Core::Input::KeyboardKey::KEY_D) == Core::Input::KeyboardKeyStatus::Pressed)
			Camera.ProcessMovement(Graphics::CAMERA_MOVEMENT_RIGHT, deltatime);

		if (Engine::GetInstance()->GetMainWindow()->GetKeyStatus(Core::Input::KeyboardKey::KEY_LEFT_SHIFT) == Core::Input::KeyboardKeyStatus::Pressed)
			Camera.MovementSpeed = 10;
		else
			Camera.MovementSpeed = 4.5;

		//Change Mouse Mode
		if (Engine::GetInstance()->GetMainWindow()->GetKeyStatus(Core::Input::KeyboardKey::KEY_ESCAPE) == Core::Input::KeyboardKeyStatus::Pressed)
		{
			isMouseDisabled = true;
			Engine::GetInstance()->GetMainWindow()->SetMouseInputMode(Core::Input::MouseInputMode::Normal);
		}
		if (Engine::GetInstance()->GetMainWindow()->GetKeyStatus(Core::Input::KeyboardKey::KEY_M) == Core::Input::KeyboardKeyStatus::Pressed)
		{
			isMouseDisabled = false;
			Engine::GetInstance()->GetMainWindow()->SetMouseInputMode(Core::Input::MouseInputMode::Virtual);
		}

		Camera.UpdateBuffer();
		mCameraSystem->Update(deltatime);
		EController.GetComponent<Components::EntityInfoComponent>()->mTransform.SetPosition(Camera.GetPosition());
	}
	void Render(float dt) override
	{
		// Clear the back buffer 
		const float ClearColor[] = { 0.350f,  0.350f,  0.350f, 1.0f };

		EController.GetComponent<Components::SpotLightComponent>()->SetDirection(Camera.GetFrontView());

		mSceneManager->Update(dt);
		{
			using namespace Graphics;
			ImGui::Begin("Sample3: Sponza Rendering");

			ImGui::Text("Press M to enable mouse capturing, or Esc to disable mouse capturing");

			//Change Rendering Pipeline
			//if (e == 0)
			//	Renderer->SetActiveRenderingPipeline(WireFrameRPPipeline.GetID());
			//else if (e == 1)
			//	Renderer->SetActiveRenderingPipeline(DiffuseRPPipeline.GetID());
			//else if (e == 2)
			//	Renderer->SetActiveRenderingPipeline(BlinnPhongPipeline.GetID());
			//else if (e == 3)
			//	Renderer->SetActiveRenderingPipeline(PBRPipeline.GetID());
			//else if (e == 4)
			//	Renderer->SetActiveRenderingPipeline(IBLPipeline.GetID());

			//else if (e == 5)
			//	Renderer->SetActiveRenderingPipeline(Deffered_BlinnPhongPipeline.GetID());
			//else if (e == 6)
			//	Renderer->SetActiveRenderingPipeline(Deffered_PBRPipeline.GetID());
			//else if (e == 7)
			//	Renderer->SetActiveRenderingPipeline(Deffered_IBLPipeline.GetID());



			ImGui::Text("Material");

			if (ImGui::Button("PBR"))
			{
				ESponza.GetComponent<Components::MeshComponent>()->SetMaterial(&SponzaPBRMaterial);
			}
			if (ImGui::Button("BlinnPhong"))
			{
				ESponza.GetComponent<Components::MeshComponent>()->SetMaterial(&SponzaBlinnPhongMaterial);
			}

			ImGui::ColorEdit3("Camera ClearColor", (float*)&Camera.RTClearColor);


			if (ImGui::TreeNode("PostFX Effects"))
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
			}

			//ImGui::Checkbox("Visualize Pointlights", &Renderer->VisualizePointLightsPositions);

			//ImGui::Checkbox("Render Skybox", &Camera.RenderSkybox);

			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

			if (ImGui::Button("End Game"))
			{
				ImGui::End();
				return Engine::GetInstance()->EndClient();
			}

			ImGui::End();
			EntityExplorer(&Scene);
		}
	}

	void Shutdown() override
	{
		mAssetManager->FlushContainers();
	}
};