#pragma once

#include <Nuclear.Platform/Input.h>

namespace Nuclear::Platform
{
	class Window;

	/// Events use platform-independent values; no native message objects escape the backend.
	enum class EventType
	{
		None,
		Quit,
		WindowClose,
		WindowResize,
		FocusChanged,
		Key,
		TextInput,
		MouseMove,
		MouseButton,
		MouseWheel,
		MouseLeave
	};

	/// One queued event. Only the fields associated with mType contain meaningful data.
	struct Event
	{
		EventType mType = EventType::None; ///< Selects which payload fields to read.
		Window* pWindow = nullptr; ///< Borrowed origin; null for process-level Quit.
		int mWidth = 0; ///< New client width in pixels for WindowResize.
		int mHeight = 0; ///< New client height in pixels for WindowResize.
		bool mFocused = false; ///< New keyboard focus state for FocusChanged.
		Input::KeyCode mKey = Input::KEYCODE_UNKNOWN; ///< Physical key for Key.
		bool mPressed = false; ///< Down/up state for Key or MouseButton.
		bool mRepeat = false; ///< True for an auto-repeated key-down event.
		std::uint32_t mCodepoint = 0; ///< Unicode scalar value for TextInput.
		int mMouseX = 0; ///< Client-relative pixel position (virtual position while locked).
		int mMouseY = 0; ///< Client-relative pixel position (virtual position while locked).
		int mDeltaX = 0; ///< Relative horizontal mouse motion in pixels.
		int mDeltaY = 0; ///< Relative vertical mouse motion in pixels.
		int mButton = 0; ///< One of MOUSE_BUTTON_* for MouseButton.
		float mWheelX = 0; ///< Horizontal wheel steps; positive means right.
		float mWheelY = 0; ///< Vertical wheel steps; positive means up.
	};
}
