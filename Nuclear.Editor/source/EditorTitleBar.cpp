#include <EditorTitleBar.h>
#include <EditorWindowButton.h>
#include <EditorWidgets.h>
#include <Assets/AssetManager.h>
#include <UI/UIContext.h>

namespace Nuclear::Editor
{
	EditorTitleBar::EditorTitleBar(Platform::Window& window) : mWindow(window)
	{
		Assets::TextureImportingDesc desc;
		desc.mCommonOptions.mAsyncImport = false;
		desc.mCommonOptions.mLoadOnly = true;
		desc.mGenerateMips = false;
		desc.mMipLevels = 1;
		auto* texture = Assets::AssetManager::Get().Import<Assets::Texture>("@NuclearAssets@/Editor/NuclearLogo.png", desc);
		auto& logo = Add<UI::Image>();
		logo.SetImageSize({28, 28});
		// Sample only the radiation emblem from the original README wordmark.
		logo.SetBrush({texture, {1, 1, 1, 1}, {0, 27.f / 150, 104.f / 800, 105.f / 150}});
		pLogo = &logo;
		pCaption = &Label(*this, "Nuclear Editor", 13);
		pCaption->SetColor({.46f, .52f, .59f, 1});
		for (int action = 0; action < 3; ++action)
			mControls[action] = &Add<EditorWindowButton>(window, static_cast<EditorWindowButton::Action>(action));
	}

	void EditorTitleBar::AddMenu(const std::string& title, EditorMenuPopup::Items items)
	{
		const size_t index = mMenus.size();
		auto& button = Action(*this, title, [this, index]() {
			if (mPopup && mOpenMenu == index) CloseMenu();
			else OpenMenu(index);
		});
		button.SetPadding({10, 10, 10, 10});
		button.SetSizeLimits({0, 40}, {1000000, 40});
		UI::ControlStyle style;
		style.Normal = {.045f, .055f, .07f, 1};
		style.Hovered = {.1f, .14f, .17f, 1};
		style.Pressed = {.06f, .22f, .23f, 1};
		button.SetStyle(style);
		mMenus.push_back({&button, std::move(items), title, {}});
	}

	void EditorTitleBar::SetMenuProvider(const std::string& title, std::function<EditorMenuPopup::Items()> provider)
	{
		for (auto& menu : mMenus)
			if (menu.Title == title)
				menu.Provider = std::move(provider);
	}

	void EditorTitleBar::SetTitle(const std::string& title)
	{
		if (pCaption->GetText() == title) return;
		pCaption->SetText(title);
		mWindow.SetTitle(title);
	}

	UI::Vec2 EditorTitleBar::OnMeasure(UI::Vec2 available)
	{
		UI::PanelWidget::OnMeasure(available);
		return {600, 40};
	}

	void EditorTitleBar::OnArrange(UI::Rect content)
	{
		pLogo->Arrange({content.x + 10, content.y + 6, 28, 28});
		float left = content.x + 48;
		const float controlsLeft = std::max(left, content.x + content.Width - 138);
		for (auto& menu : mMenus)
		{
			const float width = std::min(menu.Button->GetDesiredSize().x, std::max(0.f, controlsLeft - left));
			menu.Button->Arrange({left, content.y, width, 40});
			left += width;
		}
		pCaption->Arrange({left + 24, content.y + 12, std::max(0.f, controlsLeft - left - 36), 20});
		for (int action = 0; action < 3; ++action)
			mControls[action]->Arrange({controlsLeft + action * 46, content.y, 46, 40});
	}

	void EditorTitleBar::OnPaint(UI::DrawList& list, UI::Rect clip) const
	{
		list.Add(GetBounds(), clip, {.045f, .055f, .07f, 1});
	}

	void EditorTitleBar::OnAttached()
	{
		mWindow.SetTitleBarHitTest([this](int x, int y) {
			auto* context = GetContext();
			if (!context || !IsEnabled() || !IsVisible() || mPopup || context->GetCapturedWidget()) return false;
			context->Layout();
			const UI::Vec2 point{x / context->GetScale(), y / context->GetScale()};
			return HitTest(point, GetBounds()) == this;
		});
	}

	void EditorTitleBar::OnRemoved()
	{
		CloseMenu();
		mWindow.SetTitleBarHitTest({});
	}

	void EditorTitleBar::CloseMenu()
	{
		if (mPopup && GetContext()) GetContext()->RemoveFromViewport(std::exchange(mPopup, 0));
		mOpenMenu = size_t(-1);
	}

	void EditorTitleBar::UpdateMenu()
	{
		auto* context = GetContext();
		if (!mPopup || !context)
			return;

		if (IsEnabled() && IsVisible())
			for (auto* focused = context->Find(context->GetFocusedWidget()); focused; focused = focused->GetParent())
				if (focused->GetId() == mPopup)
					return;

		CloseMenu();
	}

	void EditorTitleBar::OpenMenu(size_t index)
	{
		CloseMenu();
		auto* context = GetContext();
		if (!context || index >= mMenus.size()) return;
		context->Layout();
		mOpenMenu = index;
		if (mMenus[index].Provider)
			mMenus[index].Items = mMenus[index].Provider();
		auto popup = std::make_unique<EditorMenuPopup>(mMenus[index].Button->GetBounds(), mMenus[index].Items,
			[this]() { CloseMenu(); },
			[this](UI::Vec2 point, bool clicked) { NavigateMenu(point, clicked); },
			[this](int direction) {
				const size_t step = direction < 0 ? mMenus.size() - 1 : 1;
				OpenMenu((mOpenMenu + step) % mMenus.size());
			});
		mPopup = popup->GetId();
		context->AddToViewport(std::move(popup), 60);
		context->SetFocus(mPopup);
	}

	void EditorTitleBar::NavigateMenu(UI::Vec2 point, bool clicked)
	{
		for (size_t index = 0; index < mMenus.size(); ++index)
			if (mMenus[index].Button->GetBounds().Contains(point))
			{
				if (index != mOpenMenu) OpenMenu(index);
				else if (clicked) CloseMenu();
				return;
			}
		if (clicked) CloseMenu();
	}
}
