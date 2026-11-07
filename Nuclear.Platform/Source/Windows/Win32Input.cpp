#include "Win32Platform.h"
#include <windowsx.h>
#include <cstddef>

namespace Nuclear::Platform::Detail
{
	// Scan-set 1 positions mapped to USB HID usages. Physical keys keep their identity
	// across keyboard layouts and Num Lock states, matching the existing managed enum.
	static constexpr std::uint16_t ScanCodes[128] =
	{
		0, 41, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 45, 46, 42, 43,
		20, 26, 8, 21, 23, 28, 24, 12, 18, 19, 47, 48, 40, 224, 4, 22,
		7, 9, 10, 11, 13, 14, 15, 51, 52, 53, 225, 49, 29, 27, 6, 25,
		5, 17, 16, 54, 55, 56, 229, 85, 226, 44, 57, 58, 59, 60, 61, 62,
		63, 64, 65, 66, 67, 72, 71, 95, 96, 97, 86, 92, 93, 94, 87, 89,
		90, 91, 98, 99, 0, 0, 100, 68, 69, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115,
		0, 0, 0, 135, 0, 0, 0, 0, 0, 0, 0, 139, 0, 137, 0, 0
	};

	static Input::KeyCode TranslateKey(WPARAM virtualKey, LPARAM data)
	{
		const unsigned scan = (data >> 16) & 0xff;
		const bool extended = (data & (1 << 24)) != 0;
		if (virtualKey == VK_PAUSE)
		{
			return Input::KEYCODE_PAUSE;
		}
		if (virtualKey == VK_SNAPSHOT)
		{
			return Input::KEYCODE_PRINTSCREEN;
		}
		if (extended)
		{
			switch (scan)
			{
			case 0x1c: return Input::KEYCODE_KP_ENTER;
			case 0x1d: return Input::KEYCODE_RCTRL;
			case 0x35: return Input::KEYCODE_KP_DIVIDE;
			case 0x38: return Input::KEYCODE_RALT;
			case 0x47: return Input::KEYCODE_HOME;
			case 0x48: return Input::KEYCODE_UP;
			case 0x49: return Input::KEYCODE_PAGEUP;
			case 0x4b: return Input::KEYCODE_LEFT;
			case 0x4d: return Input::KEYCODE_RIGHT;
			case 0x4f: return Input::KEYCODE_END;
			case 0x50: return Input::KEYCODE_DOWN;
			case 0x51: return Input::KEYCODE_PAGEDOWN;
			case 0x52: return Input::KEYCODE_INSERT;
			case 0x53: return Input::KEYCODE_DELETE;
			case 0x5b: return Input::KEYCODE_LGUI;
			case 0x5c: return Input::KEYCODE_RGUI;
			case 0x5d: return Input::KEYCODE_APPLICATION;
			}
		}
		// Media keys have device-dependent scans, so prefer their semantic virtual key.
		switch (virtualKey)
		{
		case VK_VOLUME_MUTE: return Input::KEYCODE_AUDIOMUTE;
		case VK_VOLUME_DOWN: return Input::KEYCODE_VOLUMEDOWN;
		case VK_VOLUME_UP: return Input::KEYCODE_VOLUMEUP;
		case VK_MEDIA_NEXT_TRACK: return Input::KEYCODE_AUDIONEXT;
		case VK_MEDIA_PREV_TRACK: return Input::KEYCODE_AUDIOPREV;
		case VK_MEDIA_STOP: return Input::KEYCODE_AUDIOSTOP;
		case VK_MEDIA_PLAY_PAUSE: return Input::KEYCODE_AUDIOPLAY;
		case VK_BROWSER_BACK: return Input::KEYCODE_AC_BACK;
		case VK_BROWSER_FORWARD: return Input::KEYCODE_AC_FORWARD;
		case VK_BROWSER_REFRESH: return Input::KEYCODE_AC_REFRESH;
		case VK_BROWSER_STOP: return Input::KEYCODE_AC_STOP;
		case VK_BROWSER_SEARCH: return Input::KEYCODE_AC_SEARCH;
		case VK_BROWSER_FAVORITES: return Input::KEYCODE_AC_BOOKMARKS;
		case VK_BROWSER_HOME: return Input::KEYCODE_AC_HOME;
		}
		return scan < 128 && !extended ? static_cast<Input::KeyCode>(ScanCodes[scan]) : Input::KEYCODE_UNKNOWN;
	}

	void ResetInput()
	{
		auto& state = GetState();
		state.mKeyboard.fill(0);
		state.mMouseButtons = 0;
		state.mHasMousePosition = false;
		state.mHighSurrogate = 0;
	}

	void UpdateCursor()
	{
		auto& state = GetState();
		if (!state.pFocusedWindow)
		{
			return;
		}
		auto handle = static_cast<HWND>(state.pFocusedWindow->GetNativeHandle());
		if (!handle)
		{
			return;
		}

		if (state.mMouseMode == Input::MouseInputMode::Locked && !IsIconic(handle))
		{
			RECT bounds{};
			GetClientRect(handle, &bounds);
			POINT origin{ bounds.left, bounds.top };
			POINT end{ bounds.right, bounds.bottom };
			ClientToScreen(handle, &origin);
			ClientToScreen(handle, &end);
			RECT clip{ origin.x, origin.y, end.x, end.y };
			ClipCursor(&clip);
		}
		else
		{
			ClipCursor(nullptr);
		}

		if (state.mMouseMode != Input::MouseInputMode::Normal || state.mCursor == MouseCursor::Hidden)
		{
			SetCursor(nullptr);
			return;
		}

		LPCWSTR shape = IDC_ARROW;
		switch (state.mCursor)
		{
		case MouseCursor::TextInput: shape = IDC_IBEAM; break;
		case MouseCursor::ResizeAll: shape = IDC_SIZEALL; break;
		case MouseCursor::ResizeNS: shape = IDC_SIZENS; break;
		case MouseCursor::ResizeEW: shape = IDC_SIZEWE; break;
		case MouseCursor::ResizeNESW: shape = IDC_SIZENESW; break;
		case MouseCursor::ResizeNWSE: shape = IDC_SIZENWSE; break;
		case MouseCursor::Hand: shape = IDC_HAND; break;
		case MouseCursor::NotAllowed: shape = IDC_NO; break;
		default: break;
		}
		SetCursor(LoadCursorW(nullptr, shape));
	}

	static void PushText(Window& window, std::uint32_t codepoint)
	{
		Event event;
		event.mType = EventType::TextInput;
		event.pWindow = &window;
		event.mCodepoint = codepoint;
		PushEvent(event);
	}

	bool ProcessInput(Window& window, HWND handle, UINT message, WPARAM wparam, LPARAM lparam)
	{
		auto& state = GetState();
		Event event;
		event.pWindow = &window;
		switch (message)
		{
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
			event.mType = EventType::Key;
			event.mKey = TranslateKey(wparam, lparam);
			event.mPressed = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
			event.mRepeat = event.mPressed && (lparam & (1 << 30)) != 0;
			if (event.mKey != Input::KEYCODE_UNKNOWN)
			{
				state.mKeyboard[event.mKey] = event.mPressed ? 1 : 0;
				PushEvent(event);
			}
			// Keep default handling for system keys such as Alt+F4.
			return message == WM_KEYDOWN || message == WM_KEYUP;

		case WM_CHAR:
		{
			const auto character = static_cast<char16_t>(wparam);
			if (character >= 0xd800 && character <= 0xdbff)
			{
				state.mHighSurrogate = character;
				return true;
			}
			if (character >= 0xdc00 && character <= 0xdfff)
			{
				if (state.mHighSurrogate)
				{
					PushText(window, 0x10000 + ((state.mHighSurrogate - 0xd800) << 10) + character - 0xdc00);
				}
			}
			else
			{
				PushText(window, character);
			}
			state.mHighSurrogate = 0;
			return true;
		}

		case WM_UNICHAR:
			if (wparam != UNICODE_NOCHAR && wparam <= 0x10ffff)
			{
				PushText(window, static_cast<std::uint32_t>(wparam));
			}
			return wparam != UNICODE_NOCHAR;

		case WM_MOUSEMOVE:
		{
			TRACKMOUSEEVENT track{ sizeof(track), TME_LEAVE, handle, 0 };
			TrackMouseEvent(&track);
			if (state.mMouseMode == Input::MouseInputMode::Locked && window.HasFocus())
			{
				return true;
			}
			event.mType = EventType::MouseMove;
			event.mMouseX = GET_X_LPARAM(lparam);
			event.mMouseY = GET_Y_LPARAM(lparam);
			if (state.mHasMousePosition)
			{
				event.mDeltaX = event.mMouseX - state.mMouseX;
				event.mDeltaY = event.mMouseY - state.mMouseY;
			}
			state.mMouseX = event.mMouseX;
			state.mMouseY = event.mMouseY;
			state.mHasMousePosition = true;
			PushEvent(event);
			return true;
		}

		case WM_INPUT:
			if (window.HasFocus() && state.mMouseMode == Input::MouseInputMode::Locked)
			{
				RAWINPUT input{};
				UINT bytes = sizeof(input);
				const auto result = GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT,
					&input, &bytes, sizeof(RAWINPUTHEADER));
				if (result != static_cast<UINT>(-1) && input.header.dwType == RIM_TYPEMOUSE &&
					!(input.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE))
				{
					event.mType = EventType::MouseMove;
					event.mDeltaX = input.data.mouse.lLastX;
					event.mDeltaY = input.data.mouse.lLastY;
					state.mMouseX += event.mDeltaX;
					state.mMouseY += event.mDeltaY;
					event.mMouseX = state.mMouseX;
					event.mMouseY = state.mMouseY;
					PushEvent(event);
				}
			}
			// DefWindowProc must clean up foreground raw input messages.
			return false;

		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP:
			event.mType = EventType::MouseButton;
			// Button messages carry their own client coordinates, even when no
			// WM_MOUSEMOVE preceded the click. Preserve virtual coordinates while locked.
			event.mMouseX = state.mMouseMode == Input::MouseInputMode::Locked ? state.mMouseX : GET_X_LPARAM(lparam);
			event.mMouseY = state.mMouseMode == Input::MouseInputMode::Locked ? state.mMouseY : GET_Y_LPARAM(lparam);
			event.mPressed = message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN ||
				message == WM_MBUTTONDOWN || message == WM_XBUTTONDOWN;
			if (message == WM_LBUTTONDOWN || message == WM_LBUTTONUP)
			{
				event.mButton = MOUSE_BUTTON_LEFT;
			}
			else if (message == WM_RBUTTONDOWN || message == WM_RBUTTONUP)
			{
				event.mButton = MOUSE_BUTTON_RIGHT;
			}
			else if (message == WM_MBUTTONDOWN || message == WM_MBUTTONUP)
			{
				event.mButton = MOUSE_BUTTON_MIDDLE;
			}
			else
			{
				event.mButton = GET_XBUTTON_WPARAM(wparam) == XBUTTON1 ? MOUSE_BUTTON_X1 : MOUSE_BUTTON_X2;
			}
			if (event.mPressed)
			{
				state.mMouseButtons |= MOUSE_BUTTON(event.mButton);
				SetCapture(handle);
			}
			else
			{
				state.mMouseButtons &= ~MOUSE_BUTTON(event.mButton);
				if (!state.mMouseButtons && GetCapture() == handle)
				{
					ReleaseCapture();
				}
			}
			PushEvent(event);
			return true;

		case WM_CAPTURECHANGED:
			// A modal operation or another window can steal capture during a drag.
			for (int button = MOUSE_BUTTON_LEFT; button <= MOUSE_BUTTON_X2; ++button)
			{
				if (state.mMouseButtons & MOUSE_BUTTON(button))
				{
					event.mType = EventType::MouseButton;
					event.mButton = button;
					event.mMouseX = state.mMouseX;
					event.mMouseY = state.mMouseY;
					PushEvent(event);
				}
			}
			state.mMouseButtons = 0;
			return true;

		case WM_MOUSEWHEEL:
		case WM_MOUSEHWHEEL:
			event.mType = EventType::MouseWheel;
			if (message == WM_MOUSEWHEEL)
			{
				event.mWheelY = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / WHEEL_DELTA;
			}
			else
			{
				event.mWheelX = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / WHEEL_DELTA;
			}
			PushEvent(event);
			return true;

		case WM_MOUSELEAVE:
			event.mType = EventType::MouseLeave;
			state.mHasMousePosition = false;
			PushEvent(event);
			return true;
		}
		return false;
	}
}

namespace Nuclear::Platform
{
	Input& Input::Get()
	{
		static Input input;
		return input;
	}

	const std::uint8_t* Input::GetKeyboardState()
	{
		return Detail::GetState().mKeyboard.data();
	}

	bool Input::IsKeyPressed(KeyCode key)
	{
		return key > KEYCODE_UNKNOWN && key < KEYCODE_NUM_SCANCODES && GetKeyboardState()[key] != 0;
	}

	void Input::SetMouseInputMode(const MouseInputMode& mode)
	{
		auto& state = Detail::GetState();
		if (state.mMouseMode != mode)
		{
			state.mMouseMode = mode;
			state.mHasMousePosition = false;
		}
		Detail::UpdateCursor();
	}

	Input::MouseInputMode Input::GetMouseInputMode() const
	{
		return Detail::GetState().mMouseMode;
	}

	std::uint32_t Input::GetMouseState(int* xpos, int* ypos)
	{
		auto& state = Detail::GetState();
		if (state.pFocusedWindow && state.mMouseMode != MouseInputMode::Locked)
		{
			POINT point{};
			if (GetCursorPos(&point) && ScreenToClient(static_cast<HWND>(state.pFocusedWindow->GetNativeHandle()), &point))
			{
				state.mMouseX = point.x;
				state.mMouseY = point.y;
			}
		}
		if (xpos)
		{
			*xpos = state.mMouseX;
		}
		if (ypos)
		{
			*ypos = state.mMouseY;
		}
		return state.mMouseButtons;
	}

	void Input::Update()
	{
		Detail::UpdateCursor();
	}
}
