#pragma once
#include "SampleBase.h"

class Sample3 : public SampleBase
{
	std::shared_ptr<Systems::PhysXSystem> mPhysXSystem;
	std::shared_ptr<Systems::ScriptingSystem> mScriptSystem;

	Assets::MaterialData RustedIron_D;
	Assets::MaterialData Plastic_D;

	Assets::Material RustedIron;
	Assets::Material Plastic;

	Assets::Script* script;
	Assets::Shader* PBR;
	Rendering::ForwardRenderingPath ForwardRP;

	Rendering::GeometryPass GeoPass;
	Rendering::PostProcessingPass PostFXPass;

	std::vector<ECS::Entity> boxes;

public:
	Sample3()
	{

	}
	void SetupAssets()
	{
		script = GetAssetManager().Import("@CurrentPath@/../Textures/SamplesScripts/Sample3.cs", Assets::ScriptLoadingDesc());

		//Initialize Materials
		Assets::TextureSet PBRRustedIron;
		PBRRustedIron.mData.push_back({ 0, GetAssetManager().Import("@CommonAssets@/Textures/PBR/RustedIron/albedo.png",{ Graphics::TextureUsageType::Diffuse}) });
		PBRRustedIron.mData.push_back({ 1, GetAssetManager().Import("@CommonAssets@/Textures/PBR/RustedIron/metallic.png", {Graphics::TextureUsageType::Specular}) });
		PBRRustedIron.mData.push_back({ 2, GetAssetManager().Import("@CommonAssets@/Textures/PBR/RustedIron/normal.png",{ Graphics::TextureUsageType::Normal}) });
		PBRRustedIron.mData.push_back({ 3, GetAssetManager().Import("@CommonAssets@/Textures/PBR/RustedIron/roughness.png", { Graphics::TextureUsageType::Roughness}) });
		PBRRustedIron.mData.push_back({ 4, GetAssetManager().Import("@CommonAssets@/Textures/PBR/RustedIron/ao.png", { Graphics::TextureUsageType::AO}) });

		RustedIron_D.mTextures.push_back(PBRRustedIron);
		RustedIron.SetName("RustedIron Material");

		Assets::TextureSet PBRPlastic;
		PBRPlastic.mData.push_back({ 0, GetAssetManager().Import("@CommonAssets@/Textures/PBR/plastic/albedo.png",{ Graphics::TextureUsageType::Diffuse}) });
		PBRPlastic.mData.push_back({ 1, GetAssetManager().Import("@CommonAssets@/Textures/PBR/plastic/metallic.png", {Graphics::TextureUsageType::Specular}) });
		PBRPlastic.mData.push_back({ 2, GetAssetManager().Import("@CommonAssets@/Textures/PBR/plastic/normal.png",{ Graphics::TextureUsageType::Normal}) });
		PBRPlastic.mData.push_back({ 3, GetAssetManager().Import("@CommonAssets@/Textures/PBR/plastic/roughness.png", { Graphics::TextureUsageType::Roughness}) });
		PBRPlastic.mData.push_back({ 4, GetAssetManager().Import("@CommonAssets@/Textures/PBR/plastic/ao.png", { Graphics::TextureUsageType::AO}) });

		Plastic_D.mTextures.push_back(PBRPlastic);
		Plastic.SetName("Plastic Material");

		RustedIron.Create(&RustedIron_D, PBR);
		Plastic.Create(&Plastic_D, PBR);
	}
	void SetupEntities()
	{
		auto EDirLight = GetScene().CreateEntity("DirLight");
		auto& dircomp = EDirLight.AddComponent<Components::LightComponent>(Components::LightComponent::Type::Directional);
		dircomp.SetDirection(Math::Vector3(-0.2f, -1.0f, -0.3f));
		dircomp.SetColor(Graphics::Color(0.4f, 0.4f, 0.4f, 0.0f));

		auto ELights = GetScene().CreateEntity("PointLight1");
		auto& lightcomp = ELights.AddComponent<Components::LightComponent>(Components::LightComponent::Type::Point);
		lightcomp.SetIntensity(10.0f);
				
		EController.AddComponent<Components::LightComponent>(Components::LightComponent::Type::Spot);

		ELights.GetComponent<Components::EntityInfoComponent>().mTransform.SetPosition(Math::Vector3(0.0f, 5.0f, 10.0f));
	}
	void InitRenderer()
	{
		Systems::PhysXSystemDesc sceneDesc;
		sceneDesc.mGravity = Math::Vector3(0.0f, -7.0f, 0.0f);
		mPhysXSystem = GetScene().GetSystemManager().Add<Systems::PhysXSystem>(sceneDesc);

		mScriptSystem = GetScene().GetSystemManager().Add<Systems::ScriptingSystem>();
		mScriptSystem->Initialize();

		Renderer->AddRenderPass(&GeoPass);
		Renderer->AddRenderPass(&PostFXPass);

		Assets::ShaderLoadingDesc desc;
		desc.mType = Assets::ShaderType::_3DRendering;
		PBR = GetAssetManager().Import("@NuclearAssets@/Shaders/PBR/PBR.NEShader", desc);

		Renderer->RegisterShader(PBR);

		Systems::RenderSystemBakingDesc bakedesc;
		bakedesc.RTWidth = _Width_;
		bakedesc.RTHeight = _Height_;
		Renderer->Bake(bakedesc);

		PostFXPass.Bake({ _Width_, _Height_,Rendering::RenderingEngine::GetInstance().GetFinalRT().GetDesc()});
	}

	void Load()
	{
		SampleBase::Load();

		SetupEntities();

		InitRenderer();

		SetupAssets();

		auto entity = GetScene().CreateEntity("ScriptEntity");
		auto& comp = entity.AddComponent<Components::ScriptComponent>(script);

		int nrRows = 7;
		int nrColumns = 7;
		float spacing = 2.5;
		for (int row = 0; row < nrRows; ++row)
		{
			for (int col = 0; col < nrColumns; ++col)
			{
				Math::Vector3 position(
					(float)(col - (nrColumns / 2)) * spacing,
					((float)(row - (nrRows / 2)) * spacing) + 10.0f,
					0.0f
				);


				ECS::Transform ESphere(position, Math::Vector3(2.0f));

				auto sphere = GetScene().CreateSphere(&RustedIron, ESphere);
				position.z += 5.0f;

				//ECS::Transform EBox(position, Math::Vector3(1.0f));

				//boxes.push_back(GetScene().GetFactory().CreateBox(&SphereMaterial, EBox));
			}
		}

		GetScene().CreatePlane(&Plastic);
		for (auto it : boxes)
		{
			it.GetComponent<Components::RigidBodyComponent>().isKinematic = true;
		}

		GetScene().GetMainCamera()->mRTClearColor = Graphics::Color(0.15f, 0.15f, 0.15f, 1.0f);
		//GetScene().Save();
		//GetScene().GetMainCamera()->MovementSpeed = 15;
		//Renderer->VisualizePointLightsPositions = true;


		Assets::SavedScene scene;

		GetScene().SaveScene(&scene);

		GetAssetManager().Export(&scene, "@CommonAssets@/Scenes/Sample3.bin");

		auto resultscene = GetAssetManager().Import("@CommonAssets@/Scenes/Sample3.bin", Assets::SceneLoadingDesc());

		//GetScene().LoadScene(&scene);


		Platform::Input::GetInstance().SetMouseInputMode(Platform::Input::MouseInputMode::Locked);
	}

	bool iskinematic = false;

	void Render(float dt) override
	{
		GetScene().Update(dt);

		EController.GetComponent<Components::LightComponent>().SetDirection(GetScene().GetMainCamera()->GetFrontView());


		{
			using namespace Graphics;
			ImGui::Begin("Sample3: PhysX & Scripting Test");

			ImGui::Text("Press M to enable mouse capturing, or Esc to disable mouse capturing");
		
			ImGui::ColorEdit3("Camera ClearColor", (float*)&GetScene().GetMainCamera()->mRTClearColor);
									
			PhysX::RaycastHit hit;
			if (ImGui::TreeNode("Raycast Info"))
			{
				if (Platform::Input::GetInstance().IsKeyPressed(Platform::Input::KEYCODE_F))
				{
					if (mPhysXSystem->Raycast(GetScene().GetMainCamera()->GetPosition(), GetScene().GetMainCamera()->GetFrontView(), 100.f, hit))
					{
						auto entity = hit.HitEntity;

						ImGui::Text((char*)GetScene().GetRegistry().try_get<Components::EntityInfoComponent>(entity.GetEntityID())->mName.c_str());
					}
					else
					{
						ImGui::Text("No hit");
					}
				}
				else
					ImGui::Text("Press F");

				ImGui::TreePop();
			}

			ImGui::Separator();
			ImGui::Text("PostFX Pipeline");

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

			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

			if (ImGui::Button("End Game"))
			{
				ImGui::End();
				return Core::Engine::GetInstance().EndClient();
			}

			ImGui::End();
			EntityExplorer();
		}
	}

	void Shutdown() override
	{
		GetAssetManager().FlushContainers();
	}
};