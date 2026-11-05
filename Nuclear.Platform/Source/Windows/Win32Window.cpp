#include "Win32Platform.h"
#include <algorithm>

namespace Nuclear::Platform::Detail
{
	LRESULT CALLBACK WindowProcedure(HWND handle, UINT message, WPARAM wparam, LPARAM lparam)
	{
		auto* window = reinterpret_cast<Window*>(GetWindowLongPtrW(handle, GWLP_USERDATA));
		if (message == WM_NCCREATE)
		{
			window = static_cast<Window*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
			SetWindowLongPtrW(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
		}
		if (!window)
		{
			return DefWindowProcW(handle, message, wparam, lparam);
		}

		auto& state = GetState();
		Event event;
		event.pWindow = window;
		switch (message)
		{
		case WM_CLOSE:
			// Keep the HWND alive until the client and graphics resources have shut down.
			event.mType = EventType::WindowClose;
			PushEvent(event);
			return 0;

		case WM_SIZE:
			event.mType = EventType::WindowResize;
			event.mWidth = LOWORD(lparam);
			event.mHeight = HIWORD(lparam);
			PushEvent(event);
			UpdateCursor();
			return 0;

		case WM_MOVE:
			UpdateCursor();
			break;

		case WM_SETFOCUS:
			state.pFocusedWindow = window;
			ResetInput();
			UpdateCursor();
			event.mType = EventType::FocusChanged;
			event.mFocused = true;
			PushEvent(event);
			return 0;

		case WM_KILLFOCUS:
			if (state.pFocusedWindow == window)
			{
				state.pFocusedWindow = nullptr;
				ResetInput();
				ClipCursor(nullptr);
				ReleaseCapture();
				SetCursor(LoadCursorW(nullptr, IDC_ARROW));
			}
			event.mType = EventType::FocusChanged;
			PushEvent(event);
			return 0;

		case WM_SETCURSOR:
			if (LOWORD(lparam) == HTCLIENT)
			{
				UpdateCursor();
				return TRUE;
			}
			break;

		case WM_UNICHAR:
			if (wparam == UNICODE_NOCHAR)
			{
				return TRUE;
			}
			break;

		case WM_ERASEBKGND:
			return 1;

		case WM_SYSCOMMAND:
			if ((wparam & 0xfff0) == SC_KEYMENU)
			{
				return 0;
			}
			break;
		}

		if (ProcessInput(*window, handle, message, wparam, lparam))
		{
			return 0;
		}
		return DefWindowProcW(handle, message, wparam, lparam);
	}
}

namespace Nuclear::Platform
{
	Window::~Window()
	{
		Destroy();
	}

	bool Window::Create(const WindowDesc& desc)
	{
		auto& state = Detail::GetState();
		if (pWindow || !state.mWindowClass)
		{
			state.mLastError = "Initialize the platform and use an empty Window before Create";
			return false;
		}
		if (!desc.WindowWidth || !desc.WindowHeight ||
			desc.WindowWidth > 32767 || desc.WindowHeight > 32767)
		{
			state.mLastError = "Window client dimensions must be between 1 and 32767 pixels";
			return false;
		}

		const DWORD style = desc.FullScreen ? WS_POPUP : WS_OVERLAPPEDWINDOW;
		RECT bounds{ 0, 0, static_cast<LONG>(desc.WindowWidth), static_cast<LONG>(desc.WindowHeight) };
		int x = CW_USEDEFAULT;
		int y = CW_USEDEFAULT;
		if (desc.FullScreen)
		{
			MONITORINFO monitor{ sizeof(MONITORINFO) };
			GetMonitorInfoW(MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY), &monitor);
			bounds = monitor.rcMonitor;
			x = bounds.left;
			y = bounds.top;
		}
		else
		{
			AdjustWindowRectEx(&bounds, style, FALSE, 0);
		}

		const auto title = Detail::ToWide(desc.Title);
		pWindow = CreateWindowExW(0, Detail::WindowClassName, title.c_str(), style,
			x, y, bounds.right - bounds.left, bounds.bottom - bounds.top,
			nullptr, nullptr, state.pInstance, this);
		if (!pWindow)
		{
			Detail::RecordError("CreateWindowExW");
			std::erase_if(state.mEvents, [this](const Event& event) { return event.pWindow == this; });
			return false;
		}

		state.mWindows.push_back(this);
		// A null target routes raw input to whichever platform window has focus.
		RAWINPUTDEVICE mouse{ 0x01, 0x02, 0, nullptr };
		if (!RegisterRawInputDevices(&mouse, 1, sizeof(mouse)))
		{
			Detail::RecordError("RegisterRawInputDevices");
			Destroy();
			return false;
		}
		Display();
		return true;
	}

	void Window::Destroy()
	{
		if (!pWindow)
		{
			return;
		}

		auto& state = Detail::GetState();
		DestroyWindow(static_cast<HWND>(pWindow));
		pWindow = nullptr;
		std::erase(state.mWindows, this);
		std::erase_if(state.mEvents, [this](const Event& event) { return event.pWindow == this; });
		if (state.pFocusedWindow == this)
		{
			state.pFocusedWindow = nullptr;
			Detail::ResetInput();
			ClipCursor(nullptr);
		}
		if (state.mWindows.empty())
		{
			RAWINPUTDEVICE mouse{ 0x01, 0x02, RIDEV_REMOVE, nullptr };
			RegisterRawInputDevices(&mouse, 1, sizeof(mouse));
		}
	}

	void Window::Display(bool show)
	{
		if (pWindow)
		{
			ShowWindow(static_cast<HWND>(pWindow), show ? SW_SHOW : SW_HIDE);
		}
	}

	void Window::GetSize(std::uint32_t& width, std::uint32_t& height)
	{
		int clientWidth = 0;
		int clientHeight = 0;
		GetSize(clientWidth, clientHeight);
		width = static_cast<std::uint32_t>(clientWidth);
		height = static_cast<std::uint32_t>(clientHeight);
	}

	void Window::GetSize(int& width, int& height)
	{
		RECT bounds{};
		if (pWindow)
		{
			GetClientRect(static_cast<HWND>(pWindow), &bounds);
		}
		width = bounds.right - bounds.left;
		height = bounds.bottom - bounds.top;
	}

	float Window::GetAspectRatioF32()
	{
		int width = 0;
		int height = 0;
		GetSize(width, height);
		return height > 0 ? static_cast<float>(width) / height : 0.0f;
	}

	void Window::SetSize(std::uint32_t width, std::uint32_t height)
	{
		if (!pWindow || !width || !height || width > 32767 || height > 32767)
		{
			return;
		}

		auto handle = static_cast<HWND>(pWindow);
		const auto style = static_cast<DWORD>(GetWindowLongPtrW(handle, GWL_STYLE));
		const auto extendedStyle = static_cast<DWORD>(GetWindowLongPtrW(handle, GWL_EXSTYLE));
		RECT bounds{ 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
		AdjustWindowRectEx(&bounds, style, FALSE, extendedStyle);
		SetWindowPos(handle, nullptr, 0, 0, bounds.right - bounds.left, bounds.bottom - bounds.top,
			SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	}

	void Window::SetTitle(const std::string& title)
	{
		if (pWindow)
		{
			SetWindowTextW(static_cast<HWND>(pWindow), Detail::ToWide(title).c_str());
		}
	}

	void* Window::GetNativeHandle() const
	{
		return pWindow;
	}

	bool Window::HasFocus() const
	{
		return pWindow && GetFocus() == pWindow;
	}
}
