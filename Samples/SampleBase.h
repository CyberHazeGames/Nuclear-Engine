#pragma once
#include "Common.h"

class SampleBase : public Core::Client
{
protected:
	std::shared_ptr<Systems::DebugSystem> DebugSystem;
	std::shared_ptr<Systems::RenderSystem> Renderer;

	ECS::Entity EController;

	float lastX = _Width_ / 2.0f;
	float lastY = _Height_ / 2.0f;
	bool firstMouse = true;
	bool isMouseDisabled = false;
public:
	SampleBase()
	{

	}
	virtual ~SampleBase()
	{

	}

	void Load() override
	{
		GetAssetManager().Initialize();

		//Create Controller
		EController = GetScene().CreateEntity("Controller");

		//Initialize camera
		{
			Components::CameraComponentDesc cameradesc;
			cameradesc.mProjection = Math::perspective(Math::radians(45.0f), Core::Engine::Get().GetMainWindow()->GetAspectRatioF32(), 0.1f, 100.0f);
			cameradesc.mRTDesc.mName = "CameraRT";
			cameradesc.mRTDesc = Rendering::RenderingModule::Get().GetFinalRT().GetDesc();
			cameradesc.mDepthRTDesc.mName = "CameraDepthRT";
			cameradesc.mDepthRTDesc = Rendering::RenderingModule::Get().GetFinalDepthRT().GetDesc();
			GetScene().SetMainCamera(&EController.AddComponent<Components::CameraComponent>(cameradesc));
		}

		GetScene().GetMainCamera()->mRTClearColor = Graphics::Color(0.15f, 0.15f, 0.15f, 1.0f);

		//Init systems
		Renderer = GetScene().GetSystemManager().Add<Systems::RenderSystem>();
		DebugSystem = GetScene().GetSystemManager().Add<Systems::DebugSystem>();

	}

	void OnMouseMovement(int xpos_a, int ypos_a) override
	{
		if (UI::UIModule::Get().GetContext().WantsMouse()) return;
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
		if (!UI::UIModule::Get().GetContext().WantsKeyboard())
		{
			if (Platform::Input::Get().IsKeyPressed(Platform::Input::KEYCODE_W))
				GetScene().GetMainCamera()->ProcessMovement(Components::CAMERA_MOVEMENT_FORWARD, deltatime);
			if (Platform::Input::Get().IsKeyPressed(Platform::Input::KEYCODE_A))
				GetScene().GetMainCamera()->ProcessMovement(Components::CAMERA_MOVEMENT_LEFT, deltatime);
			if (Platform::Input::Get().IsKeyPressed(Platform::Input::KEYCODE_S))
				GetScene().GetMainCamera()->ProcessMovement(Components::CAMERA_MOVEMENT_BACKWARD, deltatime);
			if (Platform::Input::Get().IsKeyPressed(Platform::Input::KEYCODE_D))
				GetScene().GetMainCamera()->ProcessMovement(Components::CAMERA_MOVEMENT_RIGHT, deltatime);

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

			if (Platform::Input::Get().IsKeyPressed(Platform::Input::KEYCODE_LSHIFT))
				GetScene().GetMainCamera()->MovementSpeed = 10;
			else
				GetScene().GetMainCamera()->MovementSpeed = 4.5;
		}

		GetScene().GetMainCamera()->UpdateBuffer();
		EController.GetComponent<Components::EntityInfoComponent>().mTransform.SetPosition(GetScene().GetMainCamera()->GetPosition());

	}


	void Shutdown() override
	{
		GetScene().Clear();
		GetAssetManager().FlushContainers();
	}
};
