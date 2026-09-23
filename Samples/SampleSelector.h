#pragma once
#include "Common.h"
#include "Sample1.h"
#include "Sample2.h"
#include "Sample3.h"
#include "Sample4.h"
#include "Sample5.h"
#include "Playground.h"

class SampleSelector : public Core::Client
{
public:
	Assets::Shader* PBR;

	void OnWindowResize(int width, int height) override
	{
		Graphics::GraphicsModule::Get().GetSwapChain()->Resize(width, height);
	}
	SampleSelector()
	{
		//	Instance = this;
	}
	void StartSample(Client* sample)
	{
		ImGui::End();
		Core::Engine::Get().EndFrame();
		Core::Engine::Get().EndClient();

		Core::Engine::Get().LoadClient(sample);
		Core::Engine::Get().EndClient();
	}

	void ImportModels()
	{
		PBR = GetAssetManager().Import<Assets::Shader>("@NuclearAssets@/Shaders/PBR/PBR.NuclearShader");

		GetAssetManager().Import<Assets::Mesh>("@CommonAssets@/Models/CrytekNanosuit/nanosuit.obj");

		GetAssetManager().Import<Assets::Mesh>("@CommonAssets@/Models/CrytekCyborg/cyborg.obj");

		GetAssetManager().Import<Assets::Mesh>("@CommonAssets@/Models/Bob/boblampclean.md5mesh");

		GetAssetManager().Import<Assets::Mesh>("@CommonAssets@/Models/vampire/dancing_vampire.dae");

		Assets::MeshImportingDesc sponza_desc;
		sponza_desc.pMaterialShader = PBR;
		GetAssetManager().Import<Assets::Mesh>("@CommonAssets@/Models/CrytekSponza/Sponza.fbx", sponza_desc);

		Assets::MeshImportingDesc ModelDesc;
		ModelDesc.mImportMaterial = false;
		ModelDesc.mImportAnimations = false;

		GetAssetManager().Import<Assets::Mesh>("@CommonAssets@/Models/shaderball/shaderball.obj", ModelDesc);
	}

	void ImportShaders()
	{

		GetAssetManager().Import<Assets::Shader>("@NuclearAssets@/Shaders/BlinnPhong.NuclearShader");
		GetAssetManager().Import<Assets::Shader>("@NuclearAssets@/Shaders/DiffuseOnly.NuclearShader");
	}

	void ImportPBRAssets()
	{
		Assets::Material RustedIron;
		Assets::Material Plastic;
		Assets::Material Grass;
		Assets::Material Gold;
		Assets::Material Wall;

		Assets::TextureImportingDesc texdesc;
		texdesc.mCommonOptions.mExportPath = "@Assets@/Textures/PBR/RustedIron/";

		Assets::MaterialTextureSet PBRRustedIron;
		PBRRustedIron.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/RustedIron/albedo.png", texdesc) , Assets::TextureUsageType::Diffuse });
		PBRRustedIron.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/RustedIron/metallic.png", texdesc) , Assets::TextureUsageType::Specular });
		PBRRustedIron.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/RustedIron/normal.png", texdesc) , Assets::TextureUsageType::Normal });
		PBRRustedIron.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/RustedIron/roughness.png", texdesc) ,  Assets::TextureUsageType::Roughness });
		PBRRustedIron.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/RustedIron/ao.png", texdesc) ,  Assets::TextureUsageType::AO });
		RustedIron.GetTextures().push_back(PBRRustedIron);
		RustedIron.SetName("RustedIron");

		texdesc.mCommonOptions.mExportPath = "@Assets@/Textures/PBR/plastic/";

		Assets::MaterialTextureSet PBRPlastic;
		PBRPlastic.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/plastic/albedo.png", texdesc) , Assets::TextureUsageType::Diffuse });
		PBRPlastic.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/plastic/metallic.png", texdesc) , Assets::TextureUsageType::Specular });
		PBRPlastic.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/plastic/normal.png", texdesc) , Assets::TextureUsageType::Normal });
		PBRPlastic.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/plastic/roughness.png", texdesc) ,  Assets::TextureUsageType::Roughness });
		PBRPlastic.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/plastic/ao.png", texdesc) ,  Assets::TextureUsageType::AO });
		Plastic.GetTextures().push_back(PBRPlastic);
		Plastic.SetName("Plastic");

		texdesc.mCommonOptions.mExportPath = "@Assets@/Textures/PBR/grass/";

		Assets::MaterialTextureSet PBRGrass;
		PBRGrass.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/grass/albedo.png", texdesc) , Assets::TextureUsageType::Diffuse });
		PBRGrass.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/grass/metallic.png", texdesc) , Assets::TextureUsageType::Specular });
		PBRGrass.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/grass/normal.png", texdesc) , Assets::TextureUsageType::Normal });
		PBRGrass.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/grass/roughness.png", texdesc) ,  Assets::TextureUsageType::Roughness });
		PBRGrass.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/grass/ao.png", texdesc) ,  Assets::TextureUsageType::AO });
		Grass.GetTextures().push_back(PBRGrass);
		Grass.SetName("Grass");

		texdesc.mCommonOptions.mExportPath = "@Assets@/Textures/PBR/wall/";

		Assets::MaterialTextureSet PBRWall;
		PBRWall.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/wall/albedo.png", texdesc) , Assets::TextureUsageType::Diffuse });
		PBRWall.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/wall/metallic.png", texdesc) , Assets::TextureUsageType::Specular });
		PBRWall.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/wall/normal.png", texdesc) , Assets::TextureUsageType::Normal });
		PBRWall.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/wall/roughness.png", texdesc) ,  Assets::TextureUsageType::Roughness });
		PBRWall.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/wall/ao.png", texdesc) ,  Assets::TextureUsageType::AO });
		Wall.GetTextures().push_back(PBRWall);
		Wall.SetName("Wall");

		texdesc.mCommonOptions.mExportPath = "@Assets@/Textures/PBR/gold/";

		Assets::MaterialTextureSet PBRGold;
		PBRGold.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/gold/albedo.png", texdesc) , Assets::TextureUsageType::Diffuse });
		PBRGold.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/gold/metallic.png", texdesc) , Assets::TextureUsageType::Specular });
		PBRGold.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/gold/normal.png", texdesc) , Assets::TextureUsageType::Normal });
		PBRGold.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/gold/roughness.png", texdesc) ,  Assets::TextureUsageType::Roughness });
		PBRGold.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/gold/ao.png", texdesc) ,  Assets::TextureUsageType::AO });
		Gold.GetTextures().push_back(PBRGold);
		Gold.SetName("Gold");

		RustedIron.Create(PBR);
		Plastic.Create(PBR);
		Grass.Create(PBR);
		Wall.Create(PBR);
		Gold.Create(PBR);


		//Export Materials
		std::string path = "@Assets@/Materials/PBR/";
		GetAssetManager().Export(&RustedIron, true, path);
		GetAssetManager().Export(&Plastic, true, path);
		GetAssetManager().Export(&Grass, true, path);
		GetAssetManager().Export(&Wall, true, path);
		GetAssetManager().Export(&Gold, true, path);
	}

	void Render(float dt) override
	{
		const float ClearColor[] = { 0.350f,  0.350f,  0.350f, 1.0f };

		auto* RTV = Graphics::GraphicsModule::Get().GetSwapChain()->GetCurrentBackBufferRTV();
		auto* DSV = Graphics::GraphicsModule::Get().GetSwapChain()->GetDepthBufferDSV();

		Graphics::GraphicsModule::Get().GetContext()->SetRenderTargets(1, &RTV, DSV, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		Graphics::GraphicsModule::Get().GetContext()->ClearRenderTarget(RTV, ClearColor, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		Graphics::GraphicsModule::Get().GetContext()->ClearDepthStencil(DSV, Diligent::CLEAR_DEPTH_FLAG, 1.0f, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

		using namespace Graphics;
		ImGui::Begin("SampleSelector");

		ImGui::Text("Select Sample");

		if (ImGui::Button("Sample1 : Basic model loading & animation & rendering"))
		{
			Sample1 demo;
			return StartSample(&demo);
		}
		if (ImGui::Button("Sample2 : Introduction to rendering pipelines and shading models."))
		{
			Sample2 demo;
			return StartSample(&demo);
		}
		if (ImGui::Button("Sample3 : PhysX & Scripting Test WIP"))             //Improve
		{
			Sample3 demo;
			return StartSample(&demo);
		}
		if (ImGui::Button("Sample4 : Multiple Cameras"))
		{
			Sample4 demo;
			return StartSample(&demo);
		}
		if (ImGui::Button("Sample5 : Async Asset Importing"))
		{
			Sample5 demo;
			return StartSample(&demo);
		}

		ImGui::Text("Select Demo");

		if (ImGui::Button("Playground"))
		{
			Playground demo;
			return StartSample(&demo);
		}

		if (ImGui::Button("Build Assets Library"))
		{
			ImportModels();
			ImportPBRAssets();
		}

		static bool vsync = false;
		ImGui::Checkbox("VSync", &vsync);
		if (vsync)
		{
			Core::Engine::Get().SetSwapChainSyncInterval(1);
		}
		else
		{
			Core::Engine::Get().SetSwapChainSyncInterval(0);
		}

		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::End();
	}
};
