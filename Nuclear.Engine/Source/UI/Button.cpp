#include "WidgetUtilities.h"
#include <Nuclear.Platform/Input.h>
#include <UI/Button.h>
#include <UI/DrawList.h>

namespace Nuclear::UI
{
	Button::Button()
	{
		SetPadding({12, 8, 12, 8});
		SetSizeLimits({60, 34}, {1000000, 1000000});
	}

	void Button::Activate()
	{
		auto callback = OnClicked;
		if (callback)
			callback();
	}

	EventReply Button::OnInput(const InputEvent& event)
	{
		using Key = Platform::Input;
		if (event.EventType == InputEvent::Type::Cancel)
		{
			mPressed = false;
			mActivationKey = 0;
			return {true, false, true};
		}
		if (event.EventType == InputEvent::Type::PointerDown && event.Button == MOUSE_BUTTON_LEFT)
		{
			mPressed = true;
			mActivationKey = 0;
			return {true, true, false, true};
		}
		if (event.EventType == InputEvent::Type::PointerUp && event.Button == MOUSE_BUTTON_LEFT && mPressed &&
			!mActivationKey)
		{
			mPressed = false;
			if (IsHovered())
				Activate();
			return {true, false, true};
		}
		if (event.EventType == InputEvent::Type::KeyDown &&
			(event.Key == Key::KEYCODE_RETURN || event.Key == Key::KEYCODE_SPACE))
		{
			if (!event.Repeat)
			{
				mPressed = true;
				mActivationKey = event.Key;
			}
			return {true};
		}
		if (event.EventType == InputEvent::Type::KeyUp && mPressed && event.Key == mActivationKey)
		{
			mPressed = false;
			mActivationKey = 0;
			Activate();
			return {true};
		}
		return {};
	}

	void Button::OnPaint(DrawList& list, Rect clip) const
	{
		const auto color = !IsEnabled() ? mStyle.Disabled
										: (mPressed ? mStyle.Pressed : (IsHovered() ? mStyle.Hovered : mStyle.Normal));
		list.Add(GetBounds(), clip, color);
		if (HasFocus())
			Detail::FocusOutline(list, GetBounds(), clip, mStyle.Focus);
	}
} // namespace Nuclear::UI
