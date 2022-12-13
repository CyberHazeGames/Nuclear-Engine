#pragma once
#include "Common.h"

std::string SplitFilename(const std::string& str)
{
	std::size_t found = str.find_last_of("/");
	return str.substr(found + 1);
}

void ImageViewer(Assets::Image* img)
{
	ImGui::Begin("Asset Info");

	ImVec2 tex_sz(128.f, 128.f);
	ImGui::Text(SplitFilename(img->GetName()).c_str());

	ImGui::Separator();
	ImGui::Image(img->mTextureView, tex_sz);

	ImGui::Text("Path: %s", img->GetName().c_str());
	ImGui::Text("Width: %i", img->mData.mWidth, " - Height: %i", img->mData.mHeight);


	ImGui::End();
}

void AssetLibraryViewer(Assets::AssetLibrary& obj)
{
	ImGui::Begin("Assets Library Viewer");
	ImVec2 tex_sz(128.f, 128.f);

	if (ImGui::BeginTabBar("Asset Library"))
	{
		ImGuiStyle& style = ImGui::GetStyle();
		float window_visible_x2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

		if (ImGui::BeginTabItem("Images"))
		{
			static int count = 0;
			static Assets::Image* img = nullptr;
			for (auto& i : obj.mImportedImages.mData)
			{
				ImGui::PushID(count);
				if (i.second.mTextureView)
				{
					ImGui::BeginGroup();
					{
						ImGui::Image(i.second.mTextureView, tex_sz);
						ImGui::Text(SplitFilename(i.second.GetName()).c_str());
						ImGui::EndGroup();
						if (ImGui::IsItemClicked())
							img = &i.second;
					}
				}
				else 
				{
					//ERROR
				}

				float last_button_x2 = ImGui::GetItemRectMax().x;
				float next_button_x2 = last_button_x2 + style.ItemSpacing.x + tex_sz.x; // Expected position if next button was on same line
				if (count + 1 < obj.mImportedImages.mData.size() && next_button_x2 < window_visible_x2)
					ImGui::SameLine();
				ImGui::PopID();

				count++;
			}
			count = 0;
			ImGui::EndTabItem();

			if (img)
			{
				if (img->isValid)
				{
					ImageViewer(img);
				}
			}
		}
		if (ImGui::BeginTabItem("Meshes"))
		{
			//Should show mesh icon only
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Materials"))
		{
			//Should show material rendered on a sphere
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	ImGui::End();
}

void AssetsFolderViewer(Assets::AssetLibrary& obj)
{

}

class Sample1 : public Core::Client
{
	std::shared_ptr<Systems::RenderSystem> Renderer;
	std::shared_ptr<Systems::CameraSystem> mCameraSystem;
//	std::shared_ptr<Systems::DebugSystem> mDebugSystem;

	Assets::Mesh* NanosuitAsset;
	Assets::Mesh* CyborgAsset;
	Assets::Mesh* BobAsset;
	Assets::Mesh* VampireAsset;

	Assets::Material CubeMaterial;
	Assets::Material NanosuitMaterial;
	Assets::Material CyborgMaterial;
	Assets::Material BobMaterial;
	Assets::Material VampireMaterial;

	Assets::MaterialData CubeTextures;
	Assets::MaterialData* NanosuitMaterialD;
	Assets::MaterialData* CyborgMaterialD;
	Assets::MaterialData* BobMaterialD;
	Assets::MaterialData* VampireMaterialD;

	Animation::Animator BobAnimator;
	Animation::Animator VampireAnimator;

	Assets::Animations* BobAnimation;
	Assets::Animations* VampireAnimation;

	Assets::Shader* DiffuseOnly;
	Assets::Shader* BlinnPhong;


	Graphics::Camera Camera;

	Rendering::Skybox Skybox;

	Rendering::ForwardRenderingPath ForwardRP;

	Rendering::GeometryPass GeoPass;

	ECS::Entity ECube;
	ECS::Entity ECyborg;
	ECS::Entity ENanosuit;
	ECS::Entity EBob;
	ECS::Entity EVampire;

	Assets::Scene Scene;
	ECS::Entity EController;

	float lastX = _Width_ / 2.0f;
	float lastY = _Height_ / 2.0f;
	bool firstMouse = true;
	bool isMouseDisabled = false;

public:
	Sample1()
		: Camera(Math::Vector3(0.0f, 0.0f, 0.0f), Math::Vector3(0.0f, 1.0f, 0.0f),  Graphics::YAW, Graphics::PITCH, Graphics::SPEED, Graphics::SENSITIVTY, Graphics::ZOOM)
	{
	}
	void SetupAssets()
	{
		Importers::MeshLoadingDesc ModelDesc;
		Assets::Animations* Placeholder;

		//Load Nanosuit Model
		std::tie(NanosuitAsset, NanosuitMaterialD, Placeholder) = Importers::AssetsImporter::GetInstance().Import("@CommonAssets@/Models/CrytekNanosuit/nanosuit.obj", ModelDesc);
		
		//Load Cyborg Model
		std::tie(CyborgAsset, CyborgMaterialD, Placeholder) = Importers::AssetsImporter::GetInstance().Import("@CommonAssets@/Models/CrytekCyborg/cyborg.obj", ModelDesc);
		
		//Load Bob Model
		std::tie(BobAsset, BobMaterialD, BobAnimation) = Importers::AssetsImporter::GetInstance().Import("@CommonAssets@/Models/Bob/boblampclean.md5mesh", ModelDesc);

		//Load Bob Model
		std::tie(VampireAsset, VampireMaterialD, VampireAnimation) = Importers::AssetsImporter::GetInstance().Import("@CommonAssets@/Models/vampire/vampire_a_lusth.fbx", ModelDesc);

		BobAnimator.Initialize(&BobAnimation->mClips.at(0));
		VampireAnimator.Initialize(&VampireAnimation->mClips.at(0));

		//Load some textures manually
		Importers::ImageLoadingDesc desc;
	//	desc.mFormat = TEX_FORMAT_RGBA8_UNORM;

		//Initialize Materials
		Assets::TextureSet CubeSet;
		CubeSet.mData.push_back({ 0, Importers::AssetsImporter::GetInstance().Import("@CommonAssets@/Textures/crate_diffuse.png",Importers::ImageLoadingDesc(), Graphics::TextureUsageType::Diffuse) });
		CubeSet.mData.push_back({ 1, Importers::AssetsImporter::GetInstance().Import("@CommonAssets@/Textures/crate_specular.png",Importers::ImageLoadingDesc(), Graphics::TextureUsageType::Specular) });
		CubeSet.mData.push_back({ 2, Importers::AssetsImporter::GetInstance().Import("@CommonAssets@/Textures/crate_normal.png",Importers::ImageLoadingDesc(), Graphics::TextureUsageType::Normal) });
		
		CubeTextures.mTextures.push_back(CubeSet);

		CubeMaterial.Create(&CubeTextures, BlinnPhong);
		NanosuitMaterial.Create(NanosuitMaterialD, BlinnPhong);
		CyborgMaterial.Create(CyborgMaterialD, BlinnPhong);
		BobMaterial.Create(BobMaterialD, DiffuseOnly);
		VampireMaterial.Create(VampireMaterialD, BlinnPhong);

		CubeSet.mData.clear();

		ECube.AddComponent<Components::MeshComponent>(Assets::DefaultMeshes::GetCubeAsset(),&CubeMaterial);
		ENanosuit.AddComponent<Components::MeshComponent>(NanosuitAsset, &NanosuitMaterial);
		ECyborg.AddComponent<Components::MeshComponent>(CyborgAsset, &CyborgMaterial);

		EBob.AddComponent<Components::MeshComponent>(BobAsset,&BobMaterial, &BobAnimator);

		//EVampire.AddComponent<Components::MeshComponent>(VampireAsset, &VampireMaterial);
		//EVampire.AddComponent<Components::AnimatorComponent>(&VampireAnimator);

		//Create The skybox
		std::array<Core::Path, 6> SkyBoxTexturePaths
		{
			Core::Path("@CommonAssets@/Skybox/right.jpg"),
			Core::Path("@CommonAssets@/Skybox/left.jpg"),
			Core::Path("@CommonAssets@/Skybox/top.jpg"),
			Core::Path("@CommonAssets@/Skybox/bottom.jpg"),
			Core::Path("@CommonAssets@/Skybox/front.jpg"),
			Core::Path("@CommonAssets@/Skybox/back.jpg")
		};

		Importers::ImageLoadingDesc SkyboxDesc;
		//SkyboxDesc.mFormat = TEX_FORMAT_RGBA8_UNORM;
		auto test = Importers::AssetsImporter::GetInstance().LoadTextureCubeFromFile(SkyBoxTexturePaths, SkyboxDesc);
		Skybox.Initialize(mCameraSystem->GetCameraCB(), test);
	}
	void SetupEntities()
	{
		//Create Entities
		ECS::Transform TNansosuit, TCyborg, TBob, TVampire, TCube;
		TNansosuit.SetPosition(Math::Vector3(0.0f, -1.75f, 0.0f));
		TNansosuit.SetScale(Math::Vector3(0.3f, 0.3f, 0.3f));

		TCyborg.SetPosition(Math::Vector3(4.0f, -1.75f, 0.0f));

		TBob.SetPosition(Math::Vector3(-4.0f, -1.75f, 0.0f));
		TBob.SetScale(Math::Vector3(0.07f, 0.07f, 0.07f));

		TVampire.SetPosition(Math::Vector3(-4.0f, -1.75f, 4.0f));
		TVampire.SetScale(Math::Vector3(0.02f, 0.02f, 0.02f));

		TCube.SetPosition(Math::Vector3(2.0f, -1.75f, 2.0f));
		TCube.SetScale(Math::Vector3(2.f, 2.f, 2.f));

		ECube = Scene.CreateEntity("Cube", TCube);
		ENanosuit = Scene.CreateEntity("Nanosuit", TNansosuit);
		ECyborg = Scene.CreateEntity("Cyborg", TCyborg);
		EBob = Scene.CreateEntity("Bob", TBob);
	//	EVampire = Scene.CreateEntity("Vampire" , TVampire);


		//Create Entities
		auto EDirLight = Scene.CreateEntity("DirLight");
		auto& dircomp = EDirLight.AddComponent<Components::LightComponent>(Components::LightComponent::Type::Directional);
		dircomp.SetDirection(Math::Vector3(-0.2f, -1.0f, -0.3f));
		dircomp.SetColor(Graphics::Color(0.4f, 0.4f, 0.4f, 0.0f));

		auto ELights = Scene.CreateEntity("PointLight1");
		auto& lightcomp = ELights.AddComponent<Components::LightComponent>(Components::LightComponent::Type::Point);

		ELights.GetComponent<Components::EntityInfoComponent>().mTransform.SetPosition(Math::Vector3(0.7f, 0.2f, 2.0f));
	}

	void InitRenderer()
	{
	//	mDebugSystem = Scene.GetSystemManager().Add<Systems::DebugSystem>();
		Renderer = Scene.GetSystemManager().Add<Systems::RenderSystem>();

		Importers::ShaderLoadingDesc desc;
		desc.mType = Importers::ShaderType::_3DRendering;
		BlinnPhong = Importers::AssetsImporter::GetInstance().Import("@NuclearAssets@/Shaders/BlinnPhong.NEShader", desc);
		DiffuseOnly = Importers::AssetsImporter::GetInstance().Import("@NuclearAssets@/Shaders/DiffuseOnly.NEShader", desc);

		Renderer->RegisterShader(BlinnPhong);
		Renderer->RegisterShader(DiffuseOnly);

		Systems::RenderSystemBakingDesc bakedesc;
		bakedesc.RTWidth = _Width_;
		bakedesc.RTHeight = _Height_;
		Renderer->Bake(bakedesc);
	}

	void Load()
	{
		Core::Engine::GetInstance().CreateScene(&Scene, true);

		Importers::AssetsImporter::GetInstance().Initialize();

		EController = Scene.CreateEntity();
		EController.AddComponent<Components::LightComponent>(Components::LightComponent::Type::Spot);
		EController.AddComponent<Components::CameraComponent>(&Camera);

		Camera.Initialize(Math::perspective(Math::radians(45.0f), Core::Engine::GetInstance().GetMainWindow()->GetAspectRatioF32(), 0.1f, 100.0f));

		mCameraSystem = Scene.GetSystemManager().Add<Systems::CameraSystem>(&Camera);

		SetupEntities();

		InitRenderer();

		SetupAssets();

		Renderer->AddRenderPass(&GeoPass);
	//	Renderer->GetBackground().SetSkybox(&Skybox);

		Platform::Input::GetInstance().SetMouseInputMode(Platform::Input::MouseInputMode::Locked);
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
		Graphics::Context::GetInstance().GetSwapChain()->Resize(width, height);
		Camera.SetProjectionMatrix(Math::perspective(Math::radians(45.0f), Core::Engine::GetInstance().GetMainWindow()->GetAspectRatioF32(), 0.1f, 100.0f));
		Renderer->ResizeRTs(width, height);
	}
	void Update(float deltatime) override
	{
		//Movement
		if (Platform::Input::GetInstance().IsKeyPressed(Platform::Input::KEYCODE_W))
			Camera.ProcessMovement(Graphics::CAMERA_MOVEMENT_FORWARD, deltatime);
		if (Platform::Input::GetInstance().IsKeyPressed(Platform::Input::KEYCODE_A))
			Camera.ProcessMovement(Graphics::CAMERA_MOVEMENT_LEFT, deltatime);
		if (Platform::Input::GetInstance().IsKeyPressed(Platform::Input::KEYCODE_S))
			Camera.ProcessMovement(Graphics::CAMERA_MOVEMENT_BACKWARD, deltatime);
		if (Platform::Input::GetInstance().IsKeyPressed(Platform::Input::KEYCODE_D))
			Camera.ProcessMovement(Graphics::CAMERA_MOVEMENT_RIGHT, deltatime);

		//if (Platform::Input::GetInstance().IsKeyPressed(Platform::Input::KEYCODE_LEFT_SHIFT))
		//	Camera.MovementSpeed = 10;
		//else
		//	Camera.MovementSpeed = 4.5;

		//Change Mouse Mode
		if (Platform::Input::GetInstance().IsKeyPressed(Platform::Input::KEYCODE_ESCAPE))
		{
			isMouseDisabled = true;
			Platform::Input::GetInstance().SetMouseInputMode(Platform::Input::MouseInputMode::Normal);
		}
		if (Platform::Input::GetInstance().IsKeyPressed(Platform::Input::KEYCODE_M))
		{
			isMouseDisabled = false;
			Platform::Input::GetInstance().SetMouseInputMode(Platform::Input::MouseInputMode::Locked);
		}

		Camera.UpdateBuffer();
		mCameraSystem->Update(deltatime);
		EController.GetComponent<Components::EntityInfoComponent>().mTransform.SetPosition(Camera.GetPosition());

	//	Renderer->GetActivePipeline()->UpdatePSO();
	}
	void Render(float dt) override
	{
		// Clear the back buffer 
		const float ClearColor[] = { 0.350f,  0.350f,  0.350f, 1.0f };
		BobAnimator.UpdateAnimation(dt);
	//	VampireAnimator.UpdateAnimation(dt);

		EController.GetComponent<Components::LightComponent>().SetDirection(Camera.GetFrontView());

		Scene.Update(dt);
		{
			using namespace Graphics;
			ImGui::Begin("Sample1: Basic Rendering");

			ImGui::Text("Press M to enable mouse capturing, or Esc to disable mouse capturing");

			ImGui::ColorEdit3("Camera ClearColor", (float*)&Camera.RTClearColor);

			//ImGui::Checkbox("Visualize Pointlights", &Renderer->VisualizePointLightsPositions);

			//ImGui::Checkbox("Render Skybox", &Camera.RenderSkybox);

			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

			if (ImGui::Button("End Game"))
			{
				ImGui::End();
				return Core::Engine::GetInstance().EndClient();
			}

			ImGui::End();
			EntityExplorer(&Scene);
			AssetLibraryViewer(Importers::AssetsImporter::GetInstance().mLibrary);

			//mDebugSystem->ShowRendertargets();
		}
	}

	void Shutdown() override
	{
		Importers::AssetsImporter::GetInstance().FlushContainers();
	}
};