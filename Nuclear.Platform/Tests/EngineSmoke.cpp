#include <Core/Engine.h>
#include <Graphics/GraphicsModule.h>
#include <Graphics/ImGui.h>
#include <Diligent/Graphics/GraphicsEngine/interface/SwapChain.h>
#include <Nuclear.Platform/Platform.h>
#include <Windows.h>
#include <iostream>

using namespace Nuclear;

class PlatformSmokeClient final : public Core::Client
{
public:
	int mFrames = 0;
	int mResizes = 0;
	int mMouseMoves = 0;
	int mScrolls = 0;

	void Update(float) override
	{
		++mFrames;
		auto* window = Core::Engine::Get().GetMainWindow();
		auto handle = static_cast<HWND>(window->GetNativeHandle());
		if (mFrames == 1)
		{
			window->Display(false);
		}
		if (mFrames == 3)
		{
			window->SetSize(960, 540);
			PostMessageW(handle, WM_MOUSEMOVE, 0, MAKELPARAM(25, 50));
			PostMessageW(handle, WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA), 0);
		}
		if (mFrames == 12)
		{
			PostMessageW(handle, WM_CLOSE, 0, 0);
		}
	}

	void Render(float) override
	{
		auto& graphics = Graphics::GraphicsModule::Get();
		auto* target = graphics.GetSwapChain()->GetCurrentBackBufferRTV();
		auto* depth = graphics.GetSwapChain()->GetDepthBufferDSV();
		graphics.GetContext()->SetRenderTargets(1, &target, depth, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		const float clearColor[] = { 0.1f, 0.15f, 0.2f, 1.0f };
		graphics.GetContext()->ClearRenderTarget(target, clearColor, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

		ImGui::Begin("Nuclear.Platform engine smoke");
		ImGui::Text("Frame %d", mFrames);
		ImGui::Button("Input and rendering integration");
		ImGui::End();
	}

	void OnWindowResize(int width, int height) override
	{
		++mResizes;
		Graphics::GraphicsModule::Get().ResizeSwapChain(width, height);
	}

	void OnMouseMovement(int, int) override
	{
		++mMouseMoves;
	}

	void OnMouseScroll(double, double) override
	{
		++mScrolls;
	}
};

int main()
{
	Core::EngineStartupDesc desc;
	desc.AutoInitAudioModule = false;
	desc.AutoInitScriptingModule = false;
	desc.AutoInitPhysXModule = false;
	desc.AutoInitThreadingModule = false;
	desc.mAssetsLibraryPath = "../Assets/";
	desc.mEngineWindowDesc.Title = "Nuclear.Platform engine validation";

	auto& engine = Core::Engine::Get();
	if (!engine.Start(desc))
	{
		std::cerr << "Engine smoke startup failed.\n";
		return 1;
	}
	PlatformSmokeClient client;
	engine.LoadClient(&client);
	engine.EndClient();
	const bool closed = engine.ShouldClose();
	engine.Shutdown();

	const bool success = closed && client.mFrames == 12 && client.mResizes >= 1 &&
		client.mMouseMoves >= 1 && client.mScrolls >= 1 && !engine.GetMainWindow()->GetNativeHandle();
	std::cout << "Frames=" << client.mFrames << " Resizes=" << client.mResizes <<
		" MouseMoves=" << client.mMouseMoves << " Scrolls=" << client.mScrolls <<
		" Closed=" << closed << "\n";
	return success ? 0 : 1;
}
