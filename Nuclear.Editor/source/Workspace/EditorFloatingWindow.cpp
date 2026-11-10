#include <EditorFloatingWindow.h>
#include <EditorTitleBar.h>
#include <EditorWidgets.h>
#include <Graphics/GraphicsModule.h>

namespace Nuclear::Editor
{
	EditorFloatingWindow::~EditorFloatingWindow() { Shutdown(); }

	bool EditorFloatingWindow::Initialize(const std::string& title, const Assets::Font* font, bool show, UI::Vec2 size)
	{
		Platform::WindowDesc desc;
		desc.Title = "Nuclear / " + title;
		desc.WindowWidth = static_cast<int>(std::max(240.f, size.x));
		desc.WindowHeight = static_cast<int>(std::max(240.f, size.y));
		desc.CustomTitleBar = true;
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
		mContext.SetViewport(float(desc.WindowWidth), float(desc.WindowHeight), mWindow.GetDpiScale());
		mContext.SetInputMode(UI::InputMode::UIOnly);
		auto root = std::make_unique<UI::VerticalBox>();
		auto& caption = root->Add<EditorTitleBar>(mWindow);
		pTitleBar = &caption;
		caption.SetTitle(desc.Title);
		auto& area = root->Add<EditorDockArea>();
		pArea = &area;
		Fill(*root, area);
		mContext.AddToViewport(std::move(root));
		return true;
	}

	void EditorFloatingWindow::SetTitle(const std::string& title)
	{
		if (pTitleBar)
			pTitleBar->SetTitle(title);
	}

	void EditorFloatingWindow::ProcessEvent(const Platform::Event& event)
	{
		if (event.pWindow != &mWindow)
			return;
		if (event.mType == Platform::EventType::WindowResize)
		{
			mContext.SetViewport(float(event.mWidth), float(event.mHeight), mWindow.GetDpiScale());
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
		pTitleBar = nullptr;
		pArea = nullptr;
		mRenderer.Shutdown();
		pSwapChain.Release();
		mWindow.Destroy();
	}
}
