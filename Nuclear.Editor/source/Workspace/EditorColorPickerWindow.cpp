#include <EditorColorPickerWindow.h>
#include <EditorColorPicker.h>
#include <EditorWidgets.h>
#include <Graphics/GraphicsModule.h>
#include <Nuclear.Platform/Input.h>
#include <utility>

namespace Nuclear::Editor
{
	EditorColorPickerWindow::~EditorColorPickerWindow() { Shutdown(); }

	void EditorColorPickerWindow::Open(UI::Widget& source, std::weak_ptr<void> lifetime, const std::string& title,
		Math::Vector4 color, bool alpha, float maximum, std::function<void(Math::Vector4)> committed)
	{
		mPending = Request{&source, std::move(lifetime), title, color, alpha, maximum, std::move(committed)};
	}

	bool EditorColorPickerWindow::IsSourceValid() const
	{
		return mActive && !mActive->Lifetime.expired() && mActive->Source->GetContext() &&
			mActive->Source->IsVisible() && mActive->Source->IsEnabled();
	}

	void EditorColorPickerWindow::Update(bool enabled)
	{
		auto request = std::exchange(mPending, {});
		if (mClose || (mActive && (!enabled || !IsSourceValid())) || request)
			Shutdown();
		if (!request || !enabled) return;
		mActive = std::move(request);
		if (!IsSourceValid()) { Shutdown(); return; }
		auto* font = mActive->Source->GetContext()->GetDefaultFont();
		pWindow = std::make_unique<EditorFloatingWindow>();
		if (!pWindow->Initialize("Color Picker / " + mActive->Title, font, true, {480, 600}))
		{
			Shutdown();
			return;
		}
		auto scroll = std::make_unique<UI::ScrollBox>();
		auto& root = scroll->Add<UI::VerticalBox>();
		root.SetPadding({18, 12, 18, 12});
		root.SetSpacing(10);
		Label(root, mActive->Title, 18);
		if (mActive->Alpha)
			pPicker = &root.Add<EditorColorPicker>(mActive->Color, std::function<void(Math::Vector4)>{}, mActive->Maximum);
		else
			pPicker = &root.Add<EditorColorPicker>(Math::Vector3(mActive->Color), std::function<void(Math::Vector3)>{}, mActive->Maximum);
		auto& actions = root.Add<UI::HorizontalBox>();
		actions.SetSpacing(8);
		Action(actions, "Apply", [this]() { Apply(); });
		Action(actions, "Cancel", [this]() { Close(); });
		auto& area = pWindow->GetArea();
		area.ShowHeader(false);
		area.AddTab({"color-picker", mActive->Title, false, false}, std::move(scroll));
		pWindow->GetContext().Layout();
	}

	void EditorColorPickerWindow::Apply()
	{
		if (!pPicker || !IsSourceValid()) { Close(); return; }
		const auto color = pPicker->GetColor();
		auto callback = mActive->Committed;
		Close();
		if (callback) callback(color);
	}

	void EditorColorPickerWindow::Close()
	{
		mPending.reset();
		mClose = true;
	}

	void EditorColorPickerWindow::Shutdown()
	{
		mPending.reset();
		mActive.reset();
		pPicker = nullptr;
		pWindow.reset();
		mClose = false;
	}

	bool EditorColorPickerWindow::ProcessEvent(const Platform::Event& event)
	{
		if (!pWindow || event.pWindow != &pWindow->GetWindow()) return false;
		if (!IsSourceValid()) { Close(); return true; }
		if (event.mType == Platform::EventType::WindowClose ||
			(event.mType == Platform::EventType::Key && event.mPressed &&
			 event.mKey == Platform::Input::KEYCODE_ESCAPE && !pWindow->GetContext().GetCapturedWidget()))
			Close();
		else
			pWindow->ProcessEvent(event);
		return true;
	}

	void EditorColorPickerWindow::Render()
	{
		if (!pWindow || mClose || !IsSourceValid()) return;
		pWindow->Render();
		pWindow->Present();
		auto& graphics = Graphics::GraphicsModule::Get();
		auto* target = graphics.GetSwapChain()->GetCurrentBackBufferRTV();
		graphics.GetContext()->SetRenderTargets(1, &target, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	}
}
