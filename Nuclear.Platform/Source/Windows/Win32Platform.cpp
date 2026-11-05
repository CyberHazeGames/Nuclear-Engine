#include "Win32Platform.h"
#include <algorithm>
#include <cstring>

namespace Nuclear::Platform::Detail
{
	State& GetState()
	{
		static State state;
		return state;
	}

	std::wstring ToWide(const std::string& text)
	{
		const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
		if (length <= 0)
		{
			return {};
		}

		std::wstring result(length, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, result.data(), length);
		result.pop_back();
		return result;
	}

	std::string ToUtf8(const std::wstring& text)
	{
		const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (length <= 0)
		{
			return {};
		}

		std::string result(length, '\0');
		WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, result.data(), length, nullptr, nullptr);
		result.pop_back();
		return result;
	}

	void RecordError(const char* operation)
	{
		const DWORD code = ::GetLastError();
		wchar_t* message = nullptr;
		FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr, code, 0, reinterpret_cast<wchar_t*>(&message), 0, nullptr);
		GetState().mLastError = std::string(operation) + " (Win32 " + std::to_string(code) + ")";
		if (message)
		{
			GetState().mLastError += ": " + ToUtf8(message);
			LocalFree(message);
		}
	}

	void PushEvent(const Event& event)
	{
		GetState().mEvents.push_back(event);
	}
}

namespace Nuclear::Platform
{
	bool Initialize()
	{
		auto& state = Detail::GetState();
		if (state.mWindowClass)
		{
			return true;
		}

		// Use the foundation DLL's module, rather than assuming ownership by the executable.
		if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCWSTR>(&Detail::WindowProcedure), &state.pInstance))
		{
			Detail::RecordError("GetModuleHandleExW");
			return false;
		}

		WNDCLASSEXW windowClass{};
		windowClass.cbSize = sizeof(windowClass);
		windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
		windowClass.lpfnWndProc = Detail::WindowProcedure;
		windowClass.hInstance = state.pInstance;
		windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
		windowClass.lpszClassName = Detail::WindowClassName;
		state.mWindowClass = RegisterClassExW(&windowClass);
		if (!state.mWindowClass)
		{
			Detail::RecordError("RegisterClassExW");
			return false;
		}

		Detail::ResetInput();
		state.mMouseMode = Input::MouseInputMode::Normal;
		state.mCursor = MouseCursor::Arrow;
		state.mLastError.clear();
		return true;
	}

	void Shutdown()
	{
		auto& state = Detail::GetState();
		if (!state.mWindows.empty())
		{
			state.mLastError = "Destroy all platform windows before Shutdown";
			return;
		}

		ClipCursor(nullptr);
		SetCursor(LoadCursorW(nullptr, IDC_ARROW));
		if (state.mWindowClass)
		{
			UnregisterClassW(Detail::WindowClassName, state.pInstance);
			state.mWindowClass = 0;
		}
		state.pFocusedWindow = nullptr;
		state.mEvents.clear();
		Detail::ResetInput();
	}

	bool PollEvent(Event& event)
	{
		auto& events = Detail::GetState().mEvents;
		if (events.empty())
		{
			MSG message{};
			while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
			{
				if (message.message == WM_QUIT)
				{
					Event quit;
					quit.mType = EventType::Quit;
					Detail::PushEvent(quit);
				}
				else
				{
					TranslateMessage(&message);
					DispatchMessageW(&message);
				}
			}
		}

		if (events.empty())
		{
			event = {};
			return false;
		}
		event = events.front();
		events.pop_front();
		return true;
	}

	std::string GetLastError()
	{
		return Detail::GetState().mLastError;
	}

	std::string GetClipboardText()
	{
		if (!OpenClipboard(nullptr))
		{
			return {};
		}

		std::string result;
		HANDLE data = GetClipboardData(CF_UNICODETEXT);
		if (data)
		{
			const auto* text = static_cast<const wchar_t*>(GlobalLock(data));
			if (text)
			{
				result = Detail::ToUtf8(text);
				GlobalUnlock(data);
			}
		}
		CloseClipboard();
		return result;
	}

	bool SetClipboardText(const std::string& text)
	{
		const auto wideText = Detail::ToWide(text);
		const auto bytes = (wideText.size() + 1) * sizeof(wchar_t);
		HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, bytes);
		if (!data)
		{
			return false;
		}

		void* destination = GlobalLock(data);
		if (!destination)
		{
			GlobalFree(data);
			return false;
		}
		std::memcpy(destination, wideText.c_str(), bytes);
		GlobalUnlock(data);

		HWND owner = nullptr;
		if (auto* window = Detail::GetState().pFocusedWindow)
		{
			owner = static_cast<HWND>(window->GetNativeHandle());
		}
		if (!owner || !OpenClipboard(owner))
		{
			GlobalFree(data);
			return false;
		}
		const bool success = EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, data);
		CloseClipboard();
		if (!success)
		{
			GlobalFree(data);
		}
		return success;
	}

	void SetMouseCursor(MouseCursor cursor)
	{
		Detail::GetState().mCursor = cursor;
		Detail::UpdateCursor();
	}

	void SetMousePosition(Window& window, int x, int y)
	{
		POINT point{ x, y };
		if (window.GetNativeHandle() && ClientToScreen(static_cast<HWND>(window.GetNativeHandle()), &point))
		{
			SetCursorPos(point.x, point.y);
		}
	}

	std::filesystem::path GetExecutablePath()
	{
		std::wstring path(32768, L'\0');
		const auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
		if (!length || length >= path.size())
		{
			Detail::RecordError("GetModuleFileNameW");
			return {};
		}
		path.resize(length);
		return std::filesystem::path(path);
	}

	void* LoadDynamicLibrary(const std::filesystem::path& path)
	{
		if (!path.is_absolute())
		{
			Detail::GetState().mLastError = "Dynamic library paths must be absolute";
			return nullptr;
		}
		auto library = LoadLibraryExW(path.c_str(), nullptr,
			LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
		if (!library)
		{
			Detail::RecordError("LoadLibraryExW");
		}
		return library;
	}

	void* GetDynamicLibrarySymbol(void* library, const char* name)
	{
		if (!library || !name)
		{
			return nullptr;
		}
		auto symbol = GetProcAddress(static_cast<HMODULE>(library), name);
		if (!symbol)
		{
			Detail::RecordError("GetProcAddress");
		}
		return reinterpret_cast<void*>(symbol);
	}

	void UnloadDynamicLibrary(void* library)
	{
		if (library)
		{
			FreeLibrary(static_cast<HMODULE>(library));
		}
	}
}
