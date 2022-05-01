#pragma once
#include "Common.h"
#include "Sample1.h"
#include "Sample2.h"
#include "Sample3.h"
#include "Sample4.h"
#include "Sample5.h"

class SampleSelector : public Game
{
public:
	//static SampleSelector* Instance;
	// 
	//Asset Manager (Loader) for all samples
	Managers::AssetManager AssetLoader;

	void OnWindowResize(int width, int height) override
	{
		Graphics::Context::GetSwapChain()->Resize(width, height);
	}
	SampleSelector()
	{
	//	Instance = this;
	}
	void StartSample()
	{
		ImGui::End();
		Engine::GetInstance()->EndFrame();

		Engine::GetInstance()->GetGame()->SetDefaultAssetManager(&AssetLoader);
		Engine::GetInstance()->LoadGame();
		Engine::GetInstance()->RunGame();
		Engine::GetInstance()->EndGame();

		//if (Engine::GetInstance()->GetGame() != this)
		//{
		//	Engine::GetInstance()->SetGame(this);
		//	Engine::GetInstance()->RunGame();
		//}
	}

	void Load()
	{
		AssetLoader.Initialize();
	}

	void Render(float dt) override
	{
		const float ClearColor[] = { 0.350f,  0.350f,  0.350f, 1.0f };

		auto* RTV = Graphics::Context::GetSwapChain()->GetCurrentBackBufferRTV();
		auto* DSV = Graphics::Context::GetSwapChain()->GetDepthBufferDSV();

		Graphics::Context::GetContext()->SetRenderTargets(1, &RTV, DSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		Graphics::Context::GetContext()->ClearRenderTarget(RTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		Graphics::Context::GetContext()->ClearDepthStencil(DSV, CLEAR_DEPTH_FLAG, 1.0f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

		using namespace Graphics;
		ImGui::Begin("SampleSelector");

		ImGui::Text("Select Sample");

		if (ImGui::Button("Sample1 : Model Loading & Animation"))
		{
			Sample1 demo;
			Engine::GetInstance()->SetGame(&demo);
			return StartSample();
		}
		if (ImGui::Button("Sample2 : PhysX Test"))
		{
			Sample2 demo;
			Engine::GetInstance()->SetGame(&demo);
			return StartSample();
		}
		if (ImGui::Button("Sample3 : Script Test"))
		{
			Sample3 demo;
			Engine::GetInstance()->SetGame(&demo);
			return StartSample();
		}
		if (ImGui::Button("Sample4 : Sponza Scene Rendering"))
		{
			Sample4 demo;
			Engine::GetInstance()->SetGame(&demo);
			return StartSample();
		}
		if (ImGui::Button("Sample5 : Advanced Rendering Effects"))
		{
			Sample5 demo;
			Engine::GetInstance()->SetGame(&demo);
			return StartSample();
		}
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::End();
	}
};