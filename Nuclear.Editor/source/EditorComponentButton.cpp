#include <EditorComponentButton.h>
#include <UI/DrawList.h>

namespace Nuclear::Editor
{
	EditorComponentButton::EditorComponentButton(Action action) : mAction(action)
	{
		SetPadding({0, 0, 0, 0});
		SetSizeLimits({24, 24}, {24, 24});
		UI::ControlStyle style;
		style.Normal = {.06f, .075f, .095f, 1};
		style.Hovered = action == Action::Remove ? UI::Color{.45f, .08f, .1f, 1} : UI::Color{.1f, .18f, .21f, 1};
		style.Pressed = {.03f, .24f, .24f, 1};
		style.Disabled = style.Normal;
		style.Focus = {.15f, .7f, .6f, 1};
		SetStyle(style);
	}

	void EditorComponentButton::SetExpanded(bool expanded)
	{
		mExpanded = expanded;
	}

	EditorComponentButton::Action EditorComponentButton::GetAction() const
	{
		return mAction;
	}

	void EditorComponentButton::OnPaint(UI::DrawList& list, UI::Rect clip) const
	{
		UI::Button::OnPaint(list, clip);
		const auto bounds = GetBounds();
		const float x = bounds.x + bounds.Width * .5f;
		const float y = bounds.y + bounds.Height * .5f;
		const UI::Color ink = IsEnabled() ? UI::Color{.72f, .79f, .86f, 1} : UI::Color{.28f, .31f, .34f, 1};
		if (mAction == Action::Remove)
		{
			for (int pixel = 0; pixel < 8; ++pixel)
			{
				list.Add({x - 4 + pixel, y - 4 + pixel, 1, 1}, clip, ink);
				list.Add({x + 3 - pixel, y - 4 + pixel, 1, 1}, clip, ink);
			}
		}
		else
		{
			for (int step = 0; step < 5; ++step)
				if (mExpanded)
					list.Add({x - 4 + step, y - 2 + step, float(9 - step * 2), 1}, clip, ink);
				else
					list.Add({x - 2 + step, y - 4 + step, 1, float(9 - step * 2)}, clip, ink);
		}
	}
}
