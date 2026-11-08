#include <EditorWindowButton.h>
#include <UI/DrawList.h>

namespace Nuclear::Editor
{
	EditorWindowButton::EditorWindowButton(Platform::Window& window, Action action)
		: mWindow(window), mAction(action)
	{
		SetPadding({0, 0, 0, 0});
		SetSizeLimits({46, 40}, {46, 40});
		UI::ControlStyle style;
		style.Normal = {.045f, .055f, .07f, 1};
		style.Hovered = action == Action::Close ? UI::Color{.65f, .025f, .04f, 1} : UI::Color{.13f, .16f, .19f, 1};
		style.Pressed = action == Action::Close ? UI::Color{.4f, .015f, .025f, 1} : UI::Color{.08f, .1f, .13f, 1};
		style.Focus = {.15f, .7f, .6f, 1};
		SetStyle(style);
		OnClicked = [this]() {
			if (mAction == Action::Minimize) mWindow.Minimize();
			else if (mAction == Action::Maximize) mWindow.ToggleMaximize();
			else mWindow.RequestClose();
		};
	}

	void EditorWindowButton::OnPaint(UI::DrawList& list, UI::Rect clip) const
	{
		UI::Button::OnPaint(list, clip);
		const auto bounds = GetBounds();
		const float x = bounds.x + bounds.Width * .5f - 5;
		const float y = bounds.y + bounds.Height * .5f - 5;
		const UI::Color ink{.78f, .82f, .87f, 1};
		auto outline = [&](float left, float top, float size) {
			list.Add({left, top, size, 1}, clip, ink);
			list.Add({left, top + size - 1, size, 1}, clip, ink);
			list.Add({left, top, 1, size}, clip, ink);
			list.Add({left + size - 1, top, 1, size}, clip, ink);
		};
		if (mAction == Action::Minimize)
			list.Add({x, y + 5, 10, 1}, clip, ink);
		else if (mAction == Action::Maximize)
		{
			if (mWindow.IsMaximized())
			{
				outline(x + 2, y, 8);
				list.Add({x, y + 2, 8, 8}, clip, {.045f, .055f, .07f, 1});
				outline(x, y + 2, 8);
			}
			else outline(x, y, 10);
		}
		else
			for (int pixel = 0; pixel < 10; ++pixel)
			{
				list.Add({x + pixel, y + pixel, 1, 1}, clip, ink);
				list.Add({x + 9 - pixel, y + pixel, 1, 1}, clip, ink);
			}
	}
}
