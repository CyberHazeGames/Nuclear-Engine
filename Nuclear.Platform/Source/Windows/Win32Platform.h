#pragma once

#include <Nuclear.Platform/Platform.h>
#include <Windows.h>
#include <array>
#include <deque>
#include <vector>

namespace Nuclear::Platform::Detail
{
	/// Internal state shared by the Windows event, window, and input implementations.
	struct State
	{
		HINSTANCE pInstance = nullptr; ///< Platform DLL instance that owns the window class.
		ATOM mWindowClass = 0; ///< Registered class; zero until Initialize succeeds.
		std::vector<Window*> mWindows; ///< Borrowed windows, all owned by callers.
		std::deque<Event> mEvents; ///< Translated messages awaiting PollEvent consumers.
		std::array<std::uint8_t, Input::KEYCODE_NUM_SCANCODES> mKeyboard{}; ///< HID-indexed keys.
		std::string mLastError; ///< Most recent backend failure in UTF-8.
		Window* pFocusedWindow = nullptr; ///< Borrowed focused window, or null.
		std::uint32_t mMouseButtons = 0; ///< Current MOUSE_BUTTON_*MASK state.
		int mMouseX = 0; ///< Current absolute or locked virtual client coordinate.
		int mMouseY = 0; ///< Current absolute or locked virtual client coordinate.
		bool mHasMousePosition = false; ///< Suppresses a spurious first relative motion.
		char16_t mHighSurrogate = 0; ///< Pending UTF-16 lead surrogate for WM_CHAR.
		Input::MouseInputMode mMouseMode = Input::MouseInputMode::Normal; ///< Requested mode.
		MouseCursor mCursor = MouseCursor::Arrow; ///< UI-requested native cursor shape.
	};

	inline constexpr wchar_t WindowClassName[] = L"Nuclear.Platform.Window";

	/// Returns the backend state, shared within the platform DLL only.
	State& GetState();
	/// Converts UTF-8 to native UTF-16; empty input produces an empty string.
	std::wstring ToWide(const std::string& text);
	/// Converts native UTF-16 to UTF-8; empty input produces an empty string.
	std::string ToUtf8(const std::wstring& text);
	/// Records an operation name and the most recent Windows error message.
	void RecordError(const char* operation);
	/// Appends one translated event to the event-thread queue.
	void PushEvent(const Event& event);
	/// Native message procedure registered for every platform window.
	LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
	/// Updates input and queues events. Returns true when the native message is handled.
	bool ProcessInput(Window& window, HWND handle, UINT message, WPARAM wparam, LPARAM lparam);
	/// Applies cursor shape and confinement for the currently focused window.
	void UpdateCursor();
	/// Clears held inputs and pending text state when focus is lost.
	void ResetInput();
}
