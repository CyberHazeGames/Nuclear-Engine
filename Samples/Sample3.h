#pragma once
#include "SampleBase.h"

class Sample3 : public SampleBase
{
	std::shared_ptr<Systems::PhysXSystem> mPhysXSystem;
	std::shared_ptr<Systems::ScriptingSystem> mScriptSystem;

	Assets::Material* RustedIron;
	Assets::Material* Plastic;

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

		script = GetAssetManager().Import<Assets::Script>("@CurrentPath@/../SamplesScripts/Sample3.cs");

		GetAssetManager().LoadFolder("@Assets@/Textures/PBR/RustedIron/");
		GetAssetManager().LoadFolder("@Assets@/Textures/PBR/Plastic/");


		//Initialize Materials
		RustedIron = GetAssetManager().Load<Assets::Material>("@Assets@/Materials/PBR/RustedIron.NEMaterial");
		Plastic = GetAssetManager().Load<Assets::Material>("@Assets@/Materials/PBR/Plastic.NEMaterial");

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

		PBR = GetAssetManager().Load<Assets::Shader>("@Assets@/Shaders/PBR.NEShader");

		Renderer->RegisterShader(PBR);

		Systems::RenderSystemBakingDesc bakedesc;
		bakedesc.RTWidth = _Width_;
		bakedesc.RTHeight = _Height_;
		Renderer->Bake(bakedesc);

		PostFXPass.Bake({ _Width_, _Height_,Rendering::RenderingModule::Get().GetFinalRT().GetDesc()});
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

				auto sphere = GetScene().CreateSphere(RustedIron, ESphere);
				position.z += 5.0f;

				//ECS::Transform EBox(position, Math::Vector3(1.0f));

				//boxes.push_back(GetScene().GetFactory().CreateBox(&SphereMaterial, EBox));
			}
		}

		GetScene().CreatePlane(Plastic);
		for (auto it : boxes)
		{
			it.GetComponent<Components::RigidBodyComponent>().isKinematic = true;
		}

		GetScene().GetMainCamera()->mRTClearColor = Graphics::Color(0.15f, 0.15f, 0.15f, 1.0f);
		//GetScene().Save();
		//GetScene().GetMainCamera()->MovementSpeed = 15;
		//Renderer->VisualizePointLightsPositions = true;


		Assets::Scene scene;

		GetScene().SaveScene(&scene);

		GetAssetManager().Export(&scene, "@CommonAssets@/Scenes/Sample3.bin");

		auto resultscene = GetAssetManager().Import<Assets::Scene>("@CommonAssets@/Scenes/Sample3.bin", Assets::SceneImportingDesc());

		//GetScene().LoadScene(&scene);

		Platform::Input::Get().SetMouseInputMode(Platform::Input::MouseInputMode::Locked);
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
				if (Platform::Input::Get().IsKeyPressed(Platform::Input::KEYCODE_F))
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
				return Core::Engine::Get().EndClient();
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
