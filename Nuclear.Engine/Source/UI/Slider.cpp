#include "WidgetUtilities.h"
#include <Nuclear.Platform/Input.h>
#include <UI/DrawList.h>
#include <UI/Slider.h>

namespace Nuclear::UI
{
	void Slider::SetValue(float value)
	{
		mValue = Detail::UnitValue(value);
	}

	void Slider::ChangeValue(float value)
	{
		const float previous = mValue;
		SetValue(value);
		auto callback = OnChanged;
		if (callback && previous != mValue)
			callback(mValue);
	}

	EventReply Slider::OnInput(const InputEvent& event)
	{
		if (event.EventType == InputEvent::Type::Cancel)
		{
			mDragging = false;
			return {true, false, true};
		}
		const bool down = event.EventType == InputEvent::Type::PointerDown && event.Button == MOUSE_BUTTON_LEFT;
		if (down || (mDragging && event.EventType == InputEvent::Type::PointerMove))
		{
			mDragging = true;
			auto bounds = ContentBounds();
			ChangeValue(bounds.Width > 12 ? (event.Position.x - bounds.x - 6) / (bounds.Width - 12) : 0);
			return {true, true, false, down};
		}
		if (mDragging && event.EventType == InputEvent::Type::PointerUp && event.Button == MOUSE_BUTTON_LEFT)
		{
			mDragging = false;
			return {true, false, true};
		}
		if (event.EventType == InputEvent::Type::KeyDown)
		{
			if (event.Key == Platform::Input::KEYCODE_LEFT)
			{
				ChangeValue(mValue - .05f);
				return {true};
			}
			if (event.Key == Platform::Input::KEYCODE_RIGHT)
			{
				ChangeValue(mValue + .05f);
				return {true};
			}
			if (event.Key == Platform::Input::KEYCODE_HOME)
			{
				ChangeValue(0);
				return {true};
			}
			if (event.Key == Platform::Input::KEYCODE_END)
			{
				ChangeValue(1);
				return {true};
			}
		}
		return {};
	}

	void Slider::OnPaint(DrawList& list, Rect clip) const
	{
		auto bounds = ContentBounds();
		list.Add({bounds.x, bounds.y + bounds.Height * .5f - 3, bounds.Width, 6}, clip, {.15f, .2f, .28f, 1});
		const Color thumb = IsEnabled() ? Color{.25f, .65f, .95f, 1} : Color{.25f, .25f, .25f, .6f};
		list.Add({bounds.x + std::max(0.0f, bounds.Width - 12) * mValue, bounds.y, 12, bounds.Height}, clip, thumb);
		if (HasFocus())
			Detail::FocusOutline(list, GetBounds(), clip, {.3f, .7f, 1, 1});
	}
} // namespace Nuclear::UI
