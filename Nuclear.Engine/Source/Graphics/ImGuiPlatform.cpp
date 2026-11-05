#include <Graphics/ImGuiPlatform.h>
#include <Graphics/ImGui.h>
#include <Nuclear.Platform/Platform.h>
#include <array>
#include <chrono>
#include <cfloat>

namespace Nuclear::Graphics::ImGuiPlatform
{
	using Platform::Input;

	struct BackendData
	{
		Platform::Window* pWindow = nullptr;
		std::chrono::steady_clock::time_point mLastFrame = std::chrono::steady_clock::now();
		std::string mClipboard;
		std::array<bool, Input::KEYCODE_NUM_SCANCODES> mKeys{};
	};

	static BackendData* GetData()
	{
		return ImGui::GetCurrentContext() ? static_cast<BackendData*>(ImGui::GetIO().BackendPlatformUserData) : nullptr;
	}

	static ImGuiKey TranslateKey(Input::KeyCode key)
	{
		if (key >= Input::KEYCODE_A && key <= Input::KEYCODE_Z)
		{
			return static_cast<ImGuiKey>(ImGuiKey_A + key - Input::KEYCODE_A);
		}
		if (key >= Input::KEYCODE_1 && key <= Input::KEYCODE_9)
		{
			return static_cast<ImGuiKey>(ImGuiKey_1 + key - Input::KEYCODE_1);
		}
		if (key >= Input::KEYCODE_F1 && key <= Input::KEYCODE_F12)
		{
			return static_cast<ImGuiKey>(ImGuiKey_F1 + key - Input::KEYCODE_F1);
		}
		if (key >= Input::KEYCODE_KP_1 && key <= Input::KEYCODE_KP_9)
		{
			return static_cast<ImGuiKey>(ImGuiKey_Keypad1 + key - Input::KEYCODE_KP_1);
		}
		switch (key)
		{
		case Input::KEYCODE_0: return ImGuiKey_0;
		case Input::KEYCODE_TAB: return ImGuiKey_Tab;
		case Input::KEYCODE_LEFT: return ImGuiKey_LeftArrow;
		case Input::KEYCODE_RIGHT: return ImGuiKey_RightArrow;
		case Input::KEYCODE_UP: return ImGuiKey_UpArrow;
		case Input::KEYCODE_DOWN: return ImGuiKey_DownArrow;
		case Input::KEYCODE_PAGEUP: return ImGuiKey_PageUp;
		case Input::KEYCODE_PAGEDOWN: return ImGuiKey_PageDown;
		case Input::KEYCODE_HOME: return ImGuiKey_Home;
		case Input::KEYCODE_END: return ImGuiKey_End;
		case Input::KEYCODE_INSERT: return ImGuiKey_Insert;
		case Input::KEYCODE_DELETE: return ImGuiKey_Delete;
		case Input::KEYCODE_BACKSPACE: return ImGuiKey_Backspace;
		case Input::KEYCODE_SPACE: return ImGuiKey_Space;
		case Input::KEYCODE_RETURN: return ImGuiKey_Enter;
		case Input::KEYCODE_ESCAPE: return ImGuiKey_Escape;
		case Input::KEYCODE_APOSTROPHE: return ImGuiKey_Apostrophe;
		case Input::KEYCODE_COMMA: return ImGuiKey_Comma;
		case Input::KEYCODE_MINUS: return ImGuiKey_Minus;
		case Input::KEYCODE_PERIOD: return ImGuiKey_Period;
		case Input::KEYCODE_SLASH: return ImGuiKey_Slash;
		case Input::KEYCODE_SEMICOLON: return ImGuiKey_Semicolon;
		case Input::KEYCODE_EQUALS: return ImGuiKey_Equal;
		case Input::KEYCODE_LEFTBRACKET: return ImGuiKey_LeftBracket;
		case Input::KEYCODE_BACKSLASH: return ImGuiKey_Backslash;
		case Input::KEYCODE_RIGHTBRACKET: return ImGuiKey_RightBracket;
		case Input::KEYCODE_GRAVE: return ImGuiKey_GraveAccent;
		case Input::KEYCODE_CAPSLOCK: return ImGuiKey_CapsLock;
		case Input::KEYCODE_SCROLLLOCK: return ImGuiKey_ScrollLock;
		case Input::KEYCODE_NUMLOCKCLEAR: return ImGuiKey_NumLock;
		case Input::KEYCODE_PRINTSCREEN: return ImGuiKey_PrintScreen;
		case Input::KEYCODE_PAUSE: return ImGuiKey_Pause;
		case Input::KEYCODE_KP_0: return ImGuiKey_Keypad0;
		case Input::KEYCODE_KP_PERIOD: return ImGuiKey_KeypadDecimal;
		case Input::KEYCODE_KP_DIVIDE: return ImGuiKey_KeypadDivide;
		case Input::KEYCODE_KP_MULTIPLY: return ImGuiKey_KeypadMultiply;
		case Input::KEYCODE_KP_MINUS: return ImGuiKey_KeypadSubtract;
		case Input::KEYCODE_KP_PLUS: return ImGuiKey_KeypadAdd;
		case Input::KEYCODE_KP_ENTER: return ImGuiKey_KeypadEnter;
		case Input::KEYCODE_KP_EQUALS: return ImGuiKey_KeypadEqual;
		case Input::KEYCODE_LCTRL: return ImGuiKey_LeftCtrl;
		case Input::KEYCODE_LSHIFT: return ImGuiKey_LeftShift;
		case Input::KEYCODE_LALT: return ImGuiKey_LeftAlt;
		case Input::KEYCODE_LGUI: return ImGuiKey_LeftSuper;
		case Input::KEYCODE_RCTRL: return ImGuiKey_RightCtrl;
		case Input::KEYCODE_RSHIFT: return ImGuiKey_RightShift;
		case Input::KEYCODE_RALT: return ImGuiKey_RightAlt;
		case Input::KEYCODE_RGUI: return ImGuiKey_RightSuper;
		case Input::KEYCODE_APPLICATION: return ImGuiKey_Menu;
		default: return ImGuiKey_None;
		}
	}

	static const char* GetClipboard(void* userData)
	{
		auto& data = *static_cast<BackendData*>(userData);
		data.mClipboard = Platform::GetClipboardText();
		return data.mClipboard.c_str();
	}

	static void SetClipboard(void*, const char* text)
	{
		Platform::SetClipboardText(text ? text : "");
	}

	bool Initialize(Platform::Window& window)
	{
		auto& io = ImGui::GetIO();
		if (io.BackendPlatformUserData || !window.GetNativeHandle())
		{
			return false;
		}

		auto* data = new BackendData();
		data->pWindow = &window;
		io.BackendPlatformUserData = data;
		io.BackendPlatformName = "Nuclear.Platform";
		io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_HasSetMousePos;
		io.GetClipboardTextFn = GetClipboard;
		io.SetClipboardTextFn = SetClipboard;
		io.ClipboardUserData = data;

		auto* viewport = ImGui::GetMainViewport();
		viewport->PlatformHandle = window.GetNativeHandle();
		viewport->PlatformHandleRaw = window.GetNativeHandle();
		return true;
	}

	void Shutdown()
	{
		auto* data = GetData();
		if (!data)
		{
			return;
		}

		auto& io = ImGui::GetIO();
		io.BackendPlatformUserData = nullptr;
		io.BackendPlatformName = nullptr;
		io.BackendFlags &= ~(ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_HasSetMousePos);
		io.GetClipboardTextFn = nullptr;
		io.SetClipboardTextFn = nullptr;
		io.ClipboardUserData = nullptr;
		ImGui::GetMainViewport()->PlatformHandle = nullptr;
		ImGui::GetMainViewport()->PlatformHandleRaw = nullptr;
		delete data;
	}

	void NewFrame()
	{
		auto* data = GetData();
		if (!data)
		{
			return;
		}

		auto& io = ImGui::GetIO();
		int width = 0;
		int height = 0;
		data->pWindow->GetSize(width, height);
		io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
		io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
		const auto now = std::chrono::steady_clock::now();
		io.DeltaTime = std::chrono::duration<float>(now - data->mLastFrame).count();
		if (io.DeltaTime <= 0.0f)
		{
			io.DeltaTime = 1.0f / 60.0f;
		}
		data->mLastFrame = now;

		if (data->pWindow->HasFocus() && Input::Get().GetMouseInputMode() == Input::MouseInputMode::Normal)
		{
			if (io.WantSetMousePos)
			{
				Platform::SetMousePosition(*data->pWindow, static_cast<int>(io.MousePos.x), static_cast<int>(io.MousePos.y));
			}
			if (!(io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange))
			{
				Platform::MouseCursor cursor = Platform::MouseCursor::Arrow;
				switch (ImGui::GetMouseCursor())
				{
				case ImGuiMouseCursor_TextInput: cursor = Platform::MouseCursor::TextInput; break;
				case ImGuiMouseCursor_ResizeAll: cursor = Platform::MouseCursor::ResizeAll; break;
				case ImGuiMouseCursor_ResizeNS: cursor = Platform::MouseCursor::ResizeNS; break;
				case ImGuiMouseCursor_ResizeEW: cursor = Platform::MouseCursor::ResizeEW; break;
				case ImGuiMouseCursor_ResizeNESW: cursor = Platform::MouseCursor::ResizeNESW; break;
				case ImGuiMouseCursor_ResizeNWSE: cursor = Platform::MouseCursor::ResizeNWSE; break;
				case ImGuiMouseCursor_Hand: cursor = Platform::MouseCursor::Hand; break;
				case ImGuiMouseCursor_NotAllowed: cursor = Platform::MouseCursor::NotAllowed; break;
				case ImGuiMouseCursor_None: cursor = Platform::MouseCursor::Hidden; break;
				}
				Platform::SetMouseCursor(io.MouseDrawCursor ? Platform::MouseCursor::Hidden : cursor);
			}
		}
	}

	void ProcessEvent(const Platform::Event& event)
	{
		auto* data = GetData();
		if (!data || event.pWindow != data->pWindow)
		{
			return;
		}

		auto& io = ImGui::GetIO();
		switch (event.mType)
		{
		case Platform::EventType::FocusChanged:
			io.AddFocusEvent(event.mFocused);
			if (!event.mFocused)
			{
				data->mKeys.fill(false);
			}
			break;

		case Platform::EventType::Key:
			if (event.mKey > Input::KEYCODE_UNKNOWN && event.mKey < Input::KEYCODE_NUM_SCANCODES)
			{
				data->mKeys[event.mKey] = event.mPressed;
				io.AddKeyEvent(ImGuiKey_ModCtrl, data->mKeys[Input::KEYCODE_LCTRL] || data->mKeys[Input::KEYCODE_RCTRL]);
				io.AddKeyEvent(ImGuiKey_ModShift, data->mKeys[Input::KEYCODE_LSHIFT] || data->mKeys[Input::KEYCODE_RSHIFT]);
				io.AddKeyEvent(ImGuiKey_ModAlt, data->mKeys[Input::KEYCODE_LALT] || data->mKeys[Input::KEYCODE_RALT]);
				io.AddKeyEvent(ImGuiKey_ModSuper, data->mKeys[Input::KEYCODE_LGUI] || data->mKeys[Input::KEYCODE_RGUI]);
				const auto key = TranslateKey(event.mKey);
				if (key != ImGuiKey_None)
				{
					io.AddKeyEvent(key, event.mPressed);
					io.SetKeyEventNativeData(key, event.mKey, event.mKey, event.mKey);
				}
			}
			break;

		case Platform::EventType::TextInput:
			io.AddInputCharacter(event.mCodepoint);
			break;

		case Platform::EventType::MouseMove:
			if (Input::Get().GetMouseInputMode() == Input::MouseInputMode::Normal)
			{
				io.AddMousePosEvent(static_cast<float>(event.mMouseX), static_cast<float>(event.mMouseY));
			}
			else
			{
				io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
			}
			break;

		case Platform::EventType::MouseButton:
		{
			// The engine uses left/middle/right order; ImGui uses left/right/middle.
			const int button = event.mButton == MOUSE_BUTTON_RIGHT ? 1 :
				event.mButton == MOUSE_BUTTON_MIDDLE ? 2 : event.mButton - 1;
			if (button >= 0 && button < 5)
			{
				io.AddMouseButtonEvent(button, event.mPressed);
			}
			break;
		}

		case Platform::EventType::MouseWheel:
			io.AddMouseWheelEvent(event.mWheelX, event.mWheelY);
			break;

		case Platform::EventType::MouseLeave:
			if (!Platform::Input::Get().GetMouseState(nullptr, nullptr))
			{
				io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
			}
			break;

		default:
			break;
		}
	}
}
