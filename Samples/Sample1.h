#pragma once
#include "Common.h"

std::string SplitFilename(const std::string& str)
{
	std::size_t found = str.find_last_of("/");
	return str.substr(found + 1);
}

void TextureViewer(Assets::Texture* img)
{
	ImGui::Begin("Asset Info");

	ImVec2 tex_sz(128.f, 128.f);
	ImGui::Text(SplitFilename(img->GetName()).c_str());

	ImGui::Separator();
	ImGui::Image(img->GetTextureView(), tex_sz);

	ImGui::Text("Path: %s", img->GetName().c_str());
	ImGui::Text("Width: %i", img->GetWidth(), " - Height: %i", img->GetHeight());


	ImGui::End();
}

void AssetLibraryViewer()
{
	ImGui::Begin("Assets Library Viewer");
	ImVec2 tex_sz(128.f, 128.f);

	if (ImGui::BeginTabBar("Asset Library"))
	{
		ImGuiStyle& style = ImGui::GetStyle();
		float window_visible_x2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

		if (ImGui::BeginTabItem("Textures"))
		{
			static int count = 0;
			static Assets::Texture* img = nullptr;
			for (auto& i : Assets::AssetLibrary::Get().mImportedTextures.mData)
			{
				ImGui::PushID(count);
				if (i.second.GetTextureView())
				{
					ImGui::BeginGroup();
					{
						ImGui::Image(i.second.GetTextureView(), tex_sz);
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
				if (count + 1 < Assets::AssetLibrary::Get().mImportedTextures.mData.size() && next_button_x2 < window_visible_x2)
					ImGui::SameLine();
				ImGui::PopID();

				count++;
			}
			count = 0;
			ImGui::EndTabItem();

			if (img)
			{
				//if (img->GetState() == Assets::Asset::State::Created)
				{
					TextureViewer(img);
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
	std::shared_ptr<Systems::AudioSystem> AudioSystem;
	std::shared_ptr<Systems::RenderSystem> Renderer;
//	std::shared_ptr<Systems::DebugSystem> mDebugSystem;


	Assets::Material CubeMaterial;

	Animation::Animator BobAnimator;
	Animation::Animator VampireAnimator;

	Assets::Shader* DiffuseOnly;
	Assets::Shader* BlinnPhong;

	Rendering::Skybox Skybox;

	Rendering::ForwardRenderingPath ForwardRP;
	Rendering::PostProcessingPass SkyboxPass;
	Rendering::GeometryPass GeoPass;

	ECS::Entity ECube;
	ECS::Entity ECyborg;
	ECS::Entity ENanosuit;
	ECS::Entity EBob;
	ECS::Entity EVampire;

	ECS::Entity EController;

	Assets::AudioClip* mBackgroundMusic;
	Assets::AudioClip* mBoxAudio;
	Assets::AudioClip* mGunFire;


	float lastX = _Width_ / 2.0f;
	float lastY = _Height_ / 2.0f;
	bool firstMouse = true;
	bool isMouseDisabled = false;

public:
	Sample1()
	{
	}
	void SetupAssets()
	{
		Assets::AudioClipImportingDesc adesc;
		adesc.mIs3D = true;
		adesc.mLoop = true;
		mBackgroundMusic = GetAssetManager().Import<Assets::AudioClip>("@Assets@/AudioClips/str3.mp3", adesc);
		//mBoxAudio = GetAssetManager().Import<Assets::AudioClip>("@Assets@/AudioClips/box_audio.mp3");


		//Load Nanosuit Model
		Assets::MeshImportingDesc desc;
		desc.mCommonOptions.mLoadOnly = true;
		desc.mCommonOptions.mAsyncImport = false;
		auto nanosuitmodel = GetAssetManager().Import<Assets::Mesh>("@CommonAssets@/Models/CrytekNanosuit/nanosuit.obj", desc);
		
		//Load Cyborg Model
		auto cyborgmodel = GetAssetManager().Import<Assets::Mesh>("@CommonAssets@/Models/CrytekCyborg/cyborg.obj", desc);
		
		//Load Bob Model
		auto bobmodel = GetAssetManager().Import<Assets::Mesh>("@CommonAssets@/Models/Bob/boblampclean.md5mesh", desc);

		//Load vampire Model
		auto vampiremodel = GetAssetManager().Import<Assets::Mesh>("@CommonAssets@/Models/vampire/dancing_vampire.dae", desc);

		BobAnimator.Initialize(bobmodel->GetImportedAnimations());
		VampireAnimator.Initialize(vampiremodel->GetImportedAnimations());

		//Load some textures manually

		//Initialize Materials
		Assets::TextureImportingDesc texdesc;
		texdesc.mCommonOptions.mLoadOnly = true;
		Assets::MaterialTextureSet CubeSet;
		CubeSet.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/crate_diffuse.png", texdesc) , Assets::TextureUsageType::Diffuse});
		CubeSet.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/crate_specular.png", texdesc) , Assets::TextureUsageType::Specular} );
		CubeSet.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/crate_normal.png", texdesc) , Assets::TextureUsageType::Normal});
		CubeMaterial.GetTextures().push_back(CubeSet);
		CubeMaterial.Create(BlinnPhong);

		nanosuitmodel->GetImportedMaterial()->Create(BlinnPhong);
		cyborgmodel->GetImportedMaterial()->Create(BlinnPhong);
		bobmodel->GetImportedMaterial()->Create(DiffuseOnly);
		vampiremodel->GetImportedMaterial()->Create(BlinnPhong);

		CubeSet.mData.clear();

		ECube.AddComponent<Components::MeshComponent>(Assets::DefaultMeshes::GetCubeAsset(),&CubeMaterial);
		ENanosuit.AddComponent<Components::MeshComponent>(nanosuitmodel, nanosuitmodel->GetImportedMaterial());
		ECyborg.AddComponent<Components::MeshComponent>(cyborgmodel, cyborgmodel->GetImportedMaterial());

		EBob.AddComponent<Components::MeshComponent>(bobmodel, bobmodel->GetImportedMaterial(), &BobAnimator);

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

		Assets::TextureImportingDesc SkyboxDesc;
		//SkyboxDesc.mFormat = TEX_FORMAT_RGBA8_UNORM;
		auto test =Assets::Importer::Get().ImportTextureCube(SkyBoxTexturePaths, SkyboxDesc);
		Skybox.Initialize(test);
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

		ECube = GetScene().CreateEntity("Cube", TCube);
		ENanosuit = GetScene().CreateEntity("Nanosuit", TNansosuit);
		ECyborg = GetScene().CreateEntity("Cyborg", TCyborg);
		EBob = GetScene().CreateEntity("Bob", TBob);
	//	EVampire = GetScene().CreateEntity("Vampire" , TVampire);


		//Create Entities
		auto EDirLight = GetScene().CreateEntity("DirLight");
		auto& dircomp = EDirLight.AddComponent<Components::LightComponent>(Components::LightComponent::Type::Directional);
		dircomp.SetDirection(Math::Vector3(-0.2f, -1.0f, -0.3f));
		dircomp.SetColor(Graphics::Color(0.4f, 0.4f, 0.4f, 0.0f));

		auto ELights = GetScene().CreateEntity("PointLight1");
		auto& lightcomp = ELights.AddComponent<Components::LightComponent>(Components::LightComponent::Type::Point);

		ELights.GetComponent<Components::EntityInfoComponent>().mTransform.SetPosition(Math::Vector3(0.7f, 0.2f, 2.0f));
	}

	void InitAudioComponents()
	{
		EController.AddComponent<Components::AudioListenerComponent>();

		//auto& bgd_audio = EController.AddComponent<Components::AudioSourceComponent>();
		//bgd_audio.SetAudioClip(mBackgroundMusic);
		
		auto& box_audio = ECube.AddComponent<Components::AudioSourceComponent>(mBackgroundMusic);
	}
	void InitRenderer()
	{
		AudioSystem = GetScene().GetSystemManager().Add<Systems::AudioSystem>();
		Renderer = GetScene().GetSystemManager().Add<Systems::RenderSystem>();

		Assets::ShaderImportingDesc desc;
		desc.mCommonOptions.mLoadOnly = true;
		desc.mCommonOptions.mAsyncImport = false;
		BlinnPhong = GetAssetManager().Import<Assets::Shader>("@NuclearAssets@/Shaders/BlinnPhong.NuclearShader", desc);
		DiffuseOnly = GetAssetManager().Import<Assets::Shader>("@NuclearAssets@/Shaders/DiffuseOnly.NuclearShader", desc);

		Renderer->RegisterShader(BlinnPhong);
		Renderer->RegisterShader(DiffuseOnly);

		Systems::RenderSystemBakingDesc bakedesc;
		bakedesc.RTWidth = _Width_;
		bakedesc.RTHeight = _Height_;
		Renderer->Bake(bakedesc);
	}

	void Load()
	{
		GetAssetManager().Initialize();

		EController = GetScene().CreateEntity();
		EController.AddComponent<Components::LightComponent>(Components::LightComponent::Type::Spot);

		//Initialize camera
		{
			Components::CameraComponentDesc cameradesc;
			cameradesc.mProjection = Math::perspective(Math::radians(45.0f), Core::Engine::Get().GetMainWindow()->GetAspectRatioF32(), 0.1f, 100.0f);
			cameradesc.mRTDesc = Rendering::RenderingModule::Get().GetFinalRT().GetDesc();
			cameradesc.mDepthRTDesc = Rendering::RenderingModule::Get().GetFinalDepthRT().GetDesc();
			GetScene().SetMainCamera(&EController.AddComponent<Components::CameraComponent>(cameradesc));
		}

		SetupEntities();

		InitRenderer();

		SetupAssets();

		InitAudioComponents();

		Renderer->AddRenderPass(&GeoPass);
	//	Renderer->(&Skybox);

		Platform::Input::Get().SetMouseInputMode(Platform::Input::MouseInputMode::Locked);
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

			GetScene().GetMainCamera()->ProcessEye(xoffset, yoffset);
		}
	}


	void OnWindowResize(int width, int height) override
	{
		Graphics::GraphicsModule::Get().GetSwapChain()->Resize(width, height);
		GetScene().GetMainCamera()->ResizeRTs(width, height);
		GetScene().GetMainCamera()->SetProjectionMatrix(Math::perspective(Math::radians(45.0f), Core::Engine::Get().GetMainWindow()->GetAspectRatioF32(), 0.1f, 100.0f));
		GetScene().GetSystemManager().ResizeRTs(width, height);
		Rendering::RenderingModule::Get().ResizeRTs(width, height);
	}
	void Update(float deltatime) override
	{
		//Movement
		if (Platform::Input::Get().IsKeyPressed(Platform::Input::KEYCODE_W))
			GetScene().GetMainCamera()->ProcessMovement(Components::CAMERA_MOVEMENT_FORWARD, deltatime);
		if (Platform::Input::Get().IsKeyPressed(Platform::Input::KEYCODE_A))
			GetScene().GetMainCamera()->ProcessMovement(Components::CAMERA_MOVEMENT_LEFT, deltatime);
		if (Platform::Input::Get().IsKeyPressed(Platform::Input::KEYCODE_S))
			GetScene().GetMainCamera()->ProcessMovement(Components::CAMERA_MOVEMENT_BACKWARD, deltatime);
		if (Platform::Input::Get().IsKeyPressed(Platform::Input::KEYCODE_D))
			GetScene().GetMainCamera()->ProcessMovement(Components::CAMERA_MOVEMENT_RIGHT, deltatime);

		if (Platform::Input::Get().IsKeyPressed(Platform::Input::KEYCODE_LSHIFT))
			GetScene().GetMainCamera()->MovementSpeed = 10;
		else
			GetScene().GetMainCamera()->MovementSpeed = 4.5;

		//Change Mouse Mode
		if (Platform::Input::Get().IsKeyPressed(Platform::Input::KEYCODE_ESCAPE))
		{
			isMouseDisabled = true;
			Platform::Input::Get().SetMouseInputMode(Platform::Input::MouseInputMode::Normal);
		}
		if (Platform::Input::Get().IsKeyPressed(Platform::Input::KEYCODE_M))
		{
			isMouseDisabled = false;
			Platform::Input::Get().SetMouseInputMode(Platform::Input::MouseInputMode::Locked);
		}

		GetScene().GetMainCamera()->UpdateBuffer();
		EController.GetComponent<Components::EntityInfoComponent>().mTransform.SetPosition(GetScene().GetMainCamera()->GetPosition());

	//	Renderer->GetActivePipeline()->UpdatePSO();
	}
	void Render(float dt) override
	{
		// Clear the back buffer 
		const float ClearColor[] = { 0.350f,  0.350f,  0.350f, 1.0f };
		BobAnimator.UpdateAnimation(dt);
	//	VampireAnimator.UpdateAnimation(dt);

		EController.GetComponent<Components::LightComponent>().SetDirection(GetScene().GetMainCamera()->GetFrontView());

		GetScene().Update(dt);
		{
			using namespace Graphics;
			ImGui::Begin("Sample1: Basic Rendering");

			ImGui::Text("Press M to enable mouse capturing, or Esc to disable mouse capturing");

			ImGui::ColorEdit3("Camera ClearColor", (float*)&GetScene().GetMainCamera()->mRTClearColor);

			//ImGui::Checkbox("Visualize Pointlights", &Renderer->VisualizePointLightsPositions);

			//ImGui::Checkbox("Render Skybox", &GetScene().GetMainCamera()->RenderSkybox);

			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

			if (ImGui::Button("End Game"))
			{
				ImGui::End();
				return Core::Engine::Get().EndClient();
			}

			static Components::AudioSourceComponent* active_audio = &ECube.GetComponent<Components::AudioSourceComponent>();;
			static int f = 0;
			if (ImGui::RadioButton("Background Audio", &f, 0))			
			//	active_audio = &EController.GetComponent<Components::AudioSourceComponent>();
						
			if (ImGui::RadioButton("Box Audio", &f, 1))			
				active_audio = &ECube.GetComponent<Components::AudioSourceComponent>();
			
			if (ImGui::Button("Play"))			
				active_audio->Play();
			
			if (ImGui::Button("Pause"))			
				active_audio->Pause();

			if (ImGui::Button("Stop"))			
				active_audio->Stop();
			
			static float v = 1.0f, v2 = 1.0f;
			if (ImGui::SliderFloat("bgd music volume: ", &v, 0.0f, 1.0f))
			{
			//	auto& bgd_audio = EController.GetComponent<Components::AudioSourceComponent>();
			//	bgd_audio.SetVolume(v);
			}
			if (ImGui::SliderFloat("box music volume: ", &v2, 0.0f, 1.0f))
			{
				auto& box_audio = ECube.GetComponent<Components::AudioSourceComponent>();
				box_audio.SetVolume(v);
			}


			ImGui::End();
			EntityExplorer();
			AssetLibraryViewer();

			//mDebugSystem->ShowRendertargets();
		}
	}

	void Shutdown() override
	{
		GetAssetManager().FlushContainers();
	}
};
