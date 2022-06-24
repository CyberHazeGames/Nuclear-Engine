#pragma once
#include "Common.h"

class Sample2 : public Client
{
	std::shared_ptr<Systems::DebugSystem> mDebugSystem;

	std::shared_ptr<Systems::RenderSystem> Renderer;
	std::shared_ptr<Systems::CameraSystem> mCameraSystem;
	std::shared_ptr<Systems::LightingSystem> mLightingSystem;

	Assets::Mesh* ShaderBall;

	Assets::Material RustedIron;
	Assets::Material Plastic;
	Assets::Material Grass;
	Assets::Material Gold;
	Assets::Material Wall;


	Graphics::Camera Camera;

	//Shading models
	Rendering::DiffuseOnly DiffuseRP;
	Rendering::WireFrame WireFrameRP;

	Rendering::PBR PBR;
	Rendering::PBR PBR_IBL;
	Rendering::PBR Deffered_PBR;
	Rendering::PBR Deffered_IBL;

	Rendering::BlinnPhong BlinnPhongRP;
	Rendering::BlinnPhong Deffered_BlinnPhong;

	//Pipelines
	Rendering::ForwardRenderingPipeline PBRPipeline;
	Rendering::ForwardRenderingPipeline BlinnPhongPipeline;
	Rendering::ForwardRenderingPipeline DiffuseRPPipeline;
	Rendering::ForwardRenderingPipeline WireFrameRPPipeline;
	Rendering::ForwardRenderingPipeline IBLPipeline;

	Rendering::DefferedRenderingPipeline Deffered_IBLPipeline;
	Rendering::DefferedRenderingPipeline Deffered_BlinnPhongPipeline;
	Rendering::DefferedRenderingPipeline Deffered_PBRPipeline;

	//IBL Settings
	Rendering::ImageBasedLighting IBL;
	Rendering::PBRCapture EnvCapture;
	Graphics::Texture HDREnv;
	Assets::Image HDR_Cube;

	Rendering::Skybox Skybox;
	ECS::Scene Scene;

	ECS::Entity EShaderBall;
	ECS::Entity ESphere;

	ECS::Entity EController;
	ECS::Entity ELights;

	std::vector<ECS::Entity> boxes;

	float lastX = _Width_ / 2.0f;
	float lastY = _Height_ / 2.0f;
	bool firstMouse = true;
	bool isMouseDisabled = false;
public:
	Sample2()
		: Camera(Math::Vector3(0.0f, 5.0f, 30.0f), Math::Vector3(0.0f, 1.0f, 0.0f), Graphics::YAW, Graphics::PITCH, 10.f, Graphics::SENSITIVTY, Graphics::ZOOM),
		PBR_IBL(&IBL),
		Deffered_IBL(&IBL)
	{

	}

	void LoadPBRMaterials()
	{
		//Initialize Materials
		Assets::TextureSet PBRRustedIron;
		PBRRustedIron.mData.push_back({ 0, mAssetManager->Import("Assets/Common/Textures/PBR/RustedIron/albedo.png",Importers::ImageLoadingDesc(), Graphics::TextureUsageType::Diffuse) });
		PBRRustedIron.mData.push_back({ 1, mAssetManager->Import("Assets/Common/Textures/PBR/RustedIron/metallic.png", Importers::ImageLoadingDesc(),Graphics::TextureUsageType::Specular) });
		PBRRustedIron.mData.push_back({ 2, mAssetManager->Import("Assets/Common/Textures/PBR/RustedIron/normal.png",Importers::ImageLoadingDesc(), Graphics::TextureUsageType::Normal) });
		PBRRustedIron.mData.push_back({ 3, mAssetManager->Import("Assets/Common/Textures/PBR/RustedIron/roughness.png", Importers::ImageLoadingDesc(), Graphics::TextureUsageType::Roughness) });
		PBRRustedIron.mData.push_back({ 4, mAssetManager->Import("Assets/Common/Textures/PBR/RustedIron/ao.png", Importers::ImageLoadingDesc(), Graphics::TextureUsageType::AO) });

		RustedIron.mPixelShaderTextures.push_back(PBRRustedIron);
		RustedIron.SetName("RustedIron Material");

		Assets::TextureSet PBRPlastic;
		PBRPlastic.mData.push_back({ 0, mAssetManager->Import("Assets/Common/Textures/PBR/plastic/albedo.png",Importers::ImageLoadingDesc(), Graphics::TextureUsageType::Diffuse) });
		PBRPlastic.mData.push_back({ 1, mAssetManager->Import("Assets/Common/Textures/PBR/plastic/metallic.png", Importers::ImageLoadingDesc(),Graphics::TextureUsageType::Specular) });
		PBRPlastic.mData.push_back({ 2, mAssetManager->Import("Assets/Common/Textures/PBR/plastic/normal.png",Importers::ImageLoadingDesc(), Graphics::TextureUsageType::Normal) });
		PBRPlastic.mData.push_back({ 3, mAssetManager->Import("Assets/Common/Textures/PBR/plastic/roughness.png", Importers::ImageLoadingDesc(), Graphics::TextureUsageType::Roughness) });
		PBRPlastic.mData.push_back({ 4, mAssetManager->Import("Assets/Common/Textures/PBR/plastic/ao.png", Importers::ImageLoadingDesc(), Graphics::TextureUsageType::AO) });

		Plastic.mPixelShaderTextures.push_back(PBRPlastic);
		Plastic.SetName("Plastic Material");

		Assets::TextureSet PBRGrass;
		PBRGrass.mData.push_back({ 0, mAssetManager->Import("Assets/Common/Textures/PBR/grass/albedo.png",Importers::ImageLoadingDesc(), Graphics::TextureUsageType::Diffuse) });
		PBRGrass.mData.push_back({ 1, mAssetManager->Import("Assets/Common/Textures/PBR/grass/metallic.png", Importers::ImageLoadingDesc(),Graphics::TextureUsageType::Specular) });
		PBRGrass.mData.push_back({ 2, mAssetManager->Import("Assets/Common/Textures/PBR/grass/normal.png",Importers::ImageLoadingDesc(), Graphics::TextureUsageType::Normal) });
		PBRGrass.mData.push_back({ 3, mAssetManager->Import("Assets/Common/Textures/PBR/grass/roughness.png", Importers::ImageLoadingDesc(), Graphics::TextureUsageType::Roughness) });
		PBRGrass.mData.push_back({ 4, mAssetManager->Import("Assets/Common/Textures/PBR/grass/ao.png", Importers::ImageLoadingDesc(), Graphics::TextureUsageType::AO) });

		Grass.mPixelShaderTextures.push_back(PBRGrass);
		Grass.SetName("Grass Material");

		Assets::TextureSet PBRWall;
		PBRWall.mData.push_back({ 0, mAssetManager->Import("Assets/Common/Textures/PBR/wall/albedo.png",Importers::ImageLoadingDesc(), Graphics::TextureUsageType::Diffuse) });
		PBRWall.mData.push_back({ 1, mAssetManager->Import("Assets/Common/Textures/PBR/wall/metallic.png", Importers::ImageLoadingDesc(),Graphics::TextureUsageType::Specular) });
		PBRWall.mData.push_back({ 2, mAssetManager->Import("Assets/Common/Textures/PBR/wall/normal.png",Importers::ImageLoadingDesc(), Graphics::TextureUsageType::Normal) });
		PBRWall.mData.push_back({ 3, mAssetManager->Import("Assets/Common/Textures/PBR/wall/roughness.png", Importers::ImageLoadingDesc(), Graphics::TextureUsageType::Roughness) });
		PBRWall.mData.push_back({ 4, mAssetManager->Import("Assets/Common/Textures/PBR/wall/ao.png", Importers::ImageLoadingDesc(), Graphics::TextureUsageType::AO) });

		Wall.mPixelShaderTextures.push_back(PBRWall);
		Wall.SetName("Wall Material");

		Assets::TextureSet PBRGold;
		PBRGold.mData.push_back({ 0, mAssetManager->Import("Assets/Common/Textures/PBR/gold/albedo.png",Importers::ImageLoadingDesc(), Graphics::TextureUsageType::Diffuse) });
		PBRGold.mData.push_back({ 1, mAssetManager->Import("Assets/Common/Textures/PBR/gold/metallic.png", Importers::ImageLoadingDesc(),Graphics::TextureUsageType::Specular) });
		PBRGold.mData.push_back({ 2, mAssetManager->Import("Assets/Common/Textures/PBR/gold/normal.png",Importers::ImageLoadingDesc(), Graphics::TextureUsageType::Normal) });
		PBRGold.mData.push_back({ 3, mAssetManager->Import("Assets/Common/Textures/PBR/gold/roughness.png", Importers::ImageLoadingDesc(), Graphics::TextureUsageType::Roughness) });
		PBRGold.mData.push_back({ 4, mAssetManager->Import("Assets/Common/Textures/PBR/gold/ao.png", Importers::ImageLoadingDesc(), Graphics::TextureUsageType::AO) });

		Gold.mPixelShaderTextures.push_back(PBRGold);
		Gold.SetName("Gold Material");
	}

	void SetupAssets()
	{
		Importers::MeshLoadingDesc ModelDesc;
		ModelDesc.LoadMaterial = false;
		Assets::Material* temp;
		Assets::Animations* temp2;

		//Load Models
		std::tie(ShaderBall, temp, temp2) = mAssetManager->Import("Assets/Common/Models/shaderball/shaderball.obj", ModelDesc);

		LoadPBRMaterials();

		Renderer->CreateMaterialForAllPipelines(&RustedIron);
		Renderer->CreateMaterialForAllPipelines(&Plastic);
		Renderer->CreateMaterialForAllPipelines(&Gold);
		Renderer->CreateMaterialForAllPipelines(&Grass);
		Renderer->CreateMaterialForAllPipelines(&Wall);

	}
	void SetupEntities()
	{
		//Create Entities
		ECS::Transform shaderballT;
		shaderballT.SetScale(0.5f);

		EShaderBall = Scene.CreateEntity("ShaderBall" , shaderballT);
		ELights = Scene.CreateEntity("Lights");
		EController = Scene.CreateEntity("Controller");

		//Assign Components
		ELights.AddComponent<Components::PointLightComponent>();
		EController.AddComponent<Components::CameraComponent>(&Camera);

		Camera.Initialize(Math::perspective(Math::radians(45.0f), Engine::GetInstance()->GetMainWindow()->GetAspectRatioF32(), 0.1f, 100.0f));
		ELights.GetComponent<Components::EntityInfoComponent>()->mTransform.SetPosition(Math::Vector3(0.0f, 5.0f, 10.0f));
		ELights.GetComponent<Components::PointLightComponent>()->SetColor(Graphics::Color(1.0f, 1.0f, 1.0f, 0.0f));
		ELights.GetComponent<Components::PointLightComponent>()->SetIntensity(10.0f);

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

	void InitForwardPipelines()
	{
		BlinnPhongPipeline.Initialize(&BlinnPhongRP, &Camera);
		DiffuseRPPipeline.Initialize(&DiffuseRP, &Camera);
		WireFrameRPPipeline.Initialize(&WireFrameRP, &Camera);
		PBRPipeline.Initialize(&PBR, &Camera);
		IBLPipeline.Initialize(&PBR_IBL, &Camera);
	}

	void InitDefferedPipelines()
	{
		Deffered_BlinnPhong.Initialize({ true });
		Deffered_PBR.Initialize({ true });
		Deffered_IBL.Initialize({ true });

		Rendering::DefferedRenderingPipelineInitInfo initInfo;
		initInfo.shadingModel = &Deffered_BlinnPhong;
		initInfo.camera = &Camera;
		Deffered_BlinnPhongPipeline.Initialize(initInfo);

		initInfo.shadingModel = &Deffered_PBR;
		Deffered_PBRPipeline.Initialize(initInfo);

		initInfo.shadingModel = &Deffered_IBL;
		initInfo.camera = &Camera;
		Deffered_IBLPipeline.Initialize(initInfo);
	}


	void InitRenderer()
	{
		mDebugSystem = Scene.GetSystemManager().Add<Systems::DebugSystem>();

		Renderer = Scene.GetSystemManager().Add<Systems::RenderSystem>();

		InitIBL();
		InitForwardPipelines();
		InitDefferedPipelines();

		Renderer->AddRenderingPipeline(&PBRPipeline);
		Renderer->AddRenderingPipeline(&BlinnPhongPipeline);
		Renderer->AddRenderingPipeline(&DiffuseRPPipeline);
		Renderer->AddRenderingPipeline(&WireFrameRPPipeline);
		Renderer->AddRenderingPipeline(&IBLPipeline);

		Renderer->AddRenderingPipeline(&Deffered_BlinnPhongPipeline);
		Renderer->AddRenderingPipeline(&Deffered_IBLPipeline);
		Renderer->AddRenderingPipeline(&Deffered_PBRPipeline);

		Renderer->Bake(_Width_, _Height_);
	}

	void Load()
	{
		mAssetManager->Initialize();

		mCameraSystem = Scene.GetSystemManager().Add<Systems::CameraSystem>(&Camera);
		mLightingSystem = Scene.GetSystemManager().Add<Systems::LightingSystem>();
		SetupEntities();
		mLightingSystem->Bake();

		InitRenderer();
		ECS::Transform sphere;
		sphere.SetScale(Math::Vector3(5.0f));
		ESphere = Scene.GetFactory().CreateSphere(&RustedIron, sphere);

		SetupAssets();

		EShaderBall.AddComponent<Components::MeshComponent>(ShaderBall, &RustedIron);
		EShaderBall.GetComponent<Components::MeshComponent>()->mRender = true;
		ESphere.GetComponent<Components::MeshComponent>()->mRender = false;

		Camera.RTClearColor = Graphics::Color(0.15f, 0.15f, 0.15f, 1.0f);

		//Renderer->VisualizePointLightsPositions = true;
		PBRPipeline.SetEffect(Utilities::Hash("HDR"), true);
		PBRPipeline.SetEffect(Utilities::Hash("GAMMACORRECTION"), true);
		IBLPipeline.SetEffect(Utilities::Hash("HDR"), true);
		IBLPipeline.SetEffect(Utilities::Hash("GAMMACORRECTION"), true);
		Deffered_PBRPipeline.SetEffect(Utilities::Hash("HDR"), true);
		Deffered_PBRPipeline.SetEffect(Utilities::Hash("GAMMACORRECTION"), true);
		Deffered_IBLPipeline.SetEffect(Utilities::Hash("HDR"), true);
		Deffered_IBLPipeline.SetEffect(Utilities::Hash("GAMMACORRECTION"), true);

		Skybox.Initialize(mCameraSystem->GetCameraCB(),&HDR_Cube);
		Renderer->GetBackground().SetSkybox(&Skybox);

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
		Camera.SetProjectionMatrix(Math::perspective(Math::radians(45.0f), Engine::GetInstance()->GetMainWindow()->GetAspectRatioF32(), 0.1f, 100.0f));
		Renderer->ResizeRenderTargets(width, height);
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

		Renderer->GetActivePipeline()->UpdatePSO();
	}
	bool iskinematic = false;

	void Render(float dt) override
	{
		Scene.Update(dt);
		{	

			using namespace Graphics;
			ImGui::Begin("Sample5: Advanced Rendering");

			static float rotationspeed = 0.0f;
			static Math::Vector3 RotationAxis = glm::vec3(0.0f, 1.0f, 0.0f);
			static ECS::Entity activeentity = EShaderBall;
			ImGui::Text("Model");

			static int f = 0;
			if (ImGui::RadioButton("ShaderBall", &f, 0))
			{
				activeentity = EShaderBall;
				EShaderBall.GetComponent<Components::MeshComponent>()->mRender = true;
				ESphere.GetComponent<Components::MeshComponent>()->mRender = false;
			}
			if (ImGui::RadioButton("Sphere", &f, 1))
			{
				activeentity = ESphere;
				EShaderBall.GetComponent<Components::MeshComponent>()->mRender = false;
				ESphere.GetComponent<Components::MeshComponent>()->mRender = true;
			}


			ImGui::Text("Material");

			if (ImGui::Button("Rusted Iron"))
			{
				activeentity.GetComponent<Components::MeshComponent>()->mMaterial = &RustedIron;
			}
			if (ImGui::Button("Wall"))
			{
				activeentity.GetComponent<Components::MeshComponent>()->mMaterial = &Wall;
			}
			if (ImGui::Button("Grass"))
			{
				activeentity.GetComponent<Components::MeshComponent>()->mMaterial = &Grass;
			}
			if (ImGui::Button("Gold"))
			{
				activeentity.GetComponent<Components::MeshComponent>()->mMaterial = &Gold;
			}
			if (ImGui::Button("Plastic"))
			{
				activeentity.GetComponent<Components::MeshComponent>()->mMaterial = &Plastic;
			}


			ImGui::SliderFloat("Rotation Speed", &rotationspeed, 0.0f, 2.0f);

			ImGui::SliderFloat3("Rotation Axis", (float*)&RotationAxis, 0.0f, 1.0f);


			float rotationAngle = LastFrame / 5.0f * rotationspeed;
			activeentity.GetComponent<Components::EntityInfoComponent>()->mTransform.SetRotation(RotationAxis, rotationAngle);


			ImGui::Text("Press M to enable mouse capturing, or Esc to disable mouse capturing");

			static int e = 3;
			ImGui::Text("Forward Rendering Pipelines:");
			ImGui::RadioButton("WireFrame", &e, 0);
			ImGui::RadioButton("DiffuseOnly", &e, 1);
			ImGui::RadioButton("BlinnPhong", &e, 2);
			ImGui::RadioButton("PBR", &e, 3);
			ImGui::RadioButton("PBR With IBL", &e, 4);

			ImGui::Text("Deffered Rendering Pipelines:");

			ImGui::RadioButton("Deffered Blinnphong", &e, 5);
			ImGui::RadioButton("Deffered PBR", &e, 6);
			ImGui::RadioButton("Deffered PBR With IBL", &e, 7);

			//Change Rendering Pipeline
			if (e == 0)
				Renderer->SetActiveRenderingPipeline(WireFrameRPPipeline.GetID());
			else if (e == 1)
				Renderer->SetActiveRenderingPipeline(DiffuseRPPipeline.GetID());
			else if (e == 2)
				Renderer->SetActiveRenderingPipeline(BlinnPhongPipeline.GetID());
			else if (e == 3)
				Renderer->SetActiveRenderingPipeline(PBRPipeline.GetID());
			else if (e == 4)
				Renderer->SetActiveRenderingPipeline(IBLPipeline.GetID());

			else if (e == 5)
				Renderer->SetActiveRenderingPipeline(Deffered_BlinnPhongPipeline.GetID());
			else if (e == 6)
				Renderer->SetActiveRenderingPipeline(Deffered_PBRPipeline.GetID());
			else if (e == 7)
				Renderer->SetActiveRenderingPipeline(Deffered_IBLPipeline.GetID());

			//ImGui::Checkbox("Visualize Pointlights", &Renderer->VisualizePointLightsPositions);

			ImGui::ColorEdit3("Camera ClearColor", (float*)&Camera.RTClearColor);

			for (auto& it : Renderer->GetActivePipeline()->mPairedEffects)
			{
				bool value = it.second.GetValue();
				ImGui::Checkbox(it.second.GetName().c_str(), &value);
				if (value != it.second.GetValue())
				{
					Renderer->GetActivePipeline()->SetEffect(it.second.GetID(), value);
				}
			}


			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

			if (ImGui::Button("End Game"))
			{
				ImGui::End();
				return Engine::GetInstance()->EndClient();
			}

			ImGui::End();
			EntityExplorer(&Scene);
			mDebugSystem->ShowRendertargets();

		}
	}

	void Shutdown() override
	{
		mAssetManager->FlushContainers();
	}
};