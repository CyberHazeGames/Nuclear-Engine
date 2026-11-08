#include <EditorFloatingWindow.h>
#include <Graphics/GraphicsModule.h>

namespace Nuclear::Editor
{
	EditorFloatingWindow::~EditorFloatingWindow() { Shutdown(); }

	bool EditorFloatingWindow::Initialize(const std::string& title, const Assets::Font* font, bool show)
	{
		Platform::WindowDesc desc;
		desc.Title = "Nuclear / " + title;
		desc.WindowWidth = 760;
		desc.WindowHeight = 620;
		if (!mWindow.Create(desc))
			return false;
		mWindow.Display(show);
		auto& graphics = Graphics::GraphicsModule::Get();
		auto swapDesc = graphics.GetSwapChain()->GetDesc();
		swapDesc.Width = desc.WindowWidth;
		swapDesc.Height = desc.WindowHeight;
		if (!graphics.CreateWindowSwapChain(mWindow, swapDesc, &pSwapChain) || !mRenderer.Initialize())
		{
			Shutdown();
			return false;
		}
		mContext.SetDefaultFont(font);
		mContext.SetViewport(float(desc.WindowWidth), float(desc.WindowHeight));
		mContext.SetInputMode(UI::InputMode::UIOnly);
		auto area = std::make_unique<EditorDockArea>();
		pArea = area.get();
		mContext.AddToViewport(std::move(area));
		return true;
	}

	void EditorFloatingWindow::ProcessEvent(const Platform::Event& event)
	{
		if (event.pWindow != &mWindow)
			return;
		if (event.mType == Platform::EventType::WindowResize)
		{
			mContext.SetViewport(float(event.mWidth), float(event.mHeight));
			if (event.mWidth > 0 && event.mHeight > 0 && pSwapChain)
				pSwapChain->Resize(event.mWidth, event.mHeight);
		}
		mContext.ProcessEvent(event);
	}

	void EditorFloatingWindow::Render()
	{
		const auto size = mContext.GetPixelSize();
		if (!pSwapChain || size.x <= 0 || size.y <= 0)
			return;
		auto& graphics = Graphics::GraphicsModule::Get();
		auto* target = pSwapChain->GetCurrentBackBufferRTV();
		graphics.GetContext()->SetRenderTargets(1, &target, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		const float clear[] = {.035f, .045f, .06f, 1};
		graphics.GetContext()->ClearRenderTarget(target, clear, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		mRenderer.Render(mContext.BuildDrawList(), size, mContext.GetScale(), target);
	}

	void EditorFloatingWindow::Present()
	{
		const auto size = mContext.GetPixelSize();
		if (pSwapChain && size.x > 0 && size.y > 0)
			pSwapChain->Present(0);
	}

	void EditorFloatingWindow::Shutdown()
	{
		mContext.Clear();
		pArea = nullptr;
		mRenderer.Shutdown();
		pSwapChain.Release();
		mWindow.Destroy();
	}
}
