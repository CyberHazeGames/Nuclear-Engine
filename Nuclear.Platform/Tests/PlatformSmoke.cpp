#include <Nuclear.Platform/Platform.h>
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace Nuclear::Platform;

static void Require(bool condition, const char* message)
{
	if (!condition)
	{
		throw std::runtime_error(message);
	}
}

static std::vector<Event> DrainEvents()
{
	std::vector<Event> events;
	Event event;
	while (PollEvent(event))
	{
		events.push_back(event);
	}
	return events;
}

static LPARAM KeyData(unsigned scan, bool extended = false, bool released = false)
{
	return 1 | (static_cast<LPARAM>(scan) << 16) | (static_cast<LPARAM>(extended) << 24) |
		(released ? (static_cast<LPARAM>(3) << 30) : 0);
}

static void CheckKey(HWND handle, unsigned virtualKey, unsigned scan, bool extended, Input::KeyCode expected)
{
	SendMessageW(handle, WM_KEYDOWN, virtualKey, KeyData(scan, extended));
	Require(Input::Get().IsKeyPressed(expected), "Physical key-down mapping failed");
	SendMessageW(handle, WM_KEYUP, virtualKey, KeyData(scan, extended, true));
	Require(!Input::Get().IsKeyPressed(expected), "Physical key-up mapping failed");

	const auto events = DrainEvents();
	Require(std::count_if(events.begin(), events.end(), [expected](const Event& event)
		{ return event.mType == EventType::Key && event.mKey == expected; }) == 2,
		"Expected one down and one up event for the mapped physical key");
}

static void RunChecks()
{
	Require(Initialize(), "Platform initialization failed");
	Require(Initialize(), "Repeated initialization failed");
	Require(GetExecutablePath().is_absolute(), "Executable discovery did not return an absolute path");
	Require(!LoadDynamicLibrary("relative.dll"), "Relative library path should be rejected");

	wchar_t systemDirectory[MAX_PATH]{};
	Require(GetSystemDirectoryW(systemDirectory, MAX_PATH) != 0, "Cannot locate system directory");
	void* library = LoadDynamicLibrary(std::filesystem::path(systemDirectory) / L"kernel32.dll");
	Require(library != nullptr, "System library loading failed");
	Require(GetDynamicLibrarySymbol(library, "GetTickCount64") != nullptr, "Library symbol lookup failed");
	Require(GetDynamicLibrarySymbol(library, "NuclearMissingSymbol") == nullptr, "Missing symbol was not rejected");
	UnloadDynamicLibrary(library);

	Window window;
	int desktopX = -17;
	int desktopY = 23;
	Require(!window.ClientToScreen(desktopX, desktopY) && !window.ScreenToClient(desktopX, desktopY) &&
		desktopX == -17 && desktopY == 23, "Empty-window coordinate conversion must fail without modifying input");
	WindowDesc desc;
	desc.WindowWidth = 640;
	desc.WindowHeight = 360;
	desc.Title = "Nuclear.Platform smoke";
	Require(window.Create(desc), "Window creation failed");
	Require(!window.Create(desc), "Creating an already-owned window should fail");
	window.Display(false);
	auto handle = static_cast<HWND>(window.GetNativeHandle());
	Require(IsWindow(handle), "Window did not expose a live HWND");
	Require(window.ClientToScreen(desktopX, desktopY) && window.ScreenToClient(desktopX, desktopY) &&
		desktopX == -17 && desktopY == 23, "Desktop/client coordinate round trip failed");
	DrainEvents();

	int width = 0;
	int height = 0;
	window.GetSize(width, height);
	Require(width == 640 && height == 360, "Client dimensions differ from the requested size");
	Require(std::abs(window.GetAspectRatioF32() - 640.0f / 360.0f) < 0.001f, "Incorrect aspect ratio");
	window.SetSize(800, 450);
	window.GetSize(width, height);
	Require(width == 800 && height == 450, "Client resize failed");
	const auto resizeEvents = DrainEvents();
	Require(std::any_of(resizeEvents.begin(), resizeEvents.end(), [](const Event& event)
		{ return event.mType == EventType::WindowResize && event.mWidth == 800 && event.mHeight == 450; }),
		"Resize event missing");

	window.SetTitle("Nuclear \xe2\x98\xa2");
	wchar_t title[64]{};
	GetWindowTextW(handle, title, 64);
	Require(std::wstring(title) == L"Nuclear \u2622", "UTF-8 title conversion failed");
	Require(SendMessageW(handle, WM_UNICHAR, UNICODE_NOCHAR, 0) == TRUE, "Unicode input capability query failed");

	CheckKey(handle, 'A', 0x1e, false, Input::KEYCODE_A);
	CheckKey(handle, VK_CONTROL, 0x1d, false, Input::KEYCODE_LCTRL);
	CheckKey(handle, VK_CONTROL, 0x1d, true, Input::KEYCODE_RCTRL);
	CheckKey(handle, VK_SHIFT, 0x36, false, Input::KEYCODE_RSHIFT);
	CheckKey(handle, VK_RETURN, 0x1c, true, Input::KEYCODE_KP_ENTER);
	CheckKey(handle, VK_HOME, 0x47, false, Input::KEYCODE_KP_7);
	CheckKey(handle, VK_HOME, 0x47, true, Input::KEYCODE_HOME);
	CheckKey(handle, VK_F12, 0x58, false, Input::KEYCODE_F12);
	Require(!Input::Get().IsKeyPressed(static_cast<Input::KeyCode>(-1)), "Negative key value was accepted");
	Require(!Input::Get().IsKeyPressed(Input::KEYCODE_NUM_SCANCODES), "Out-of-range key value was accepted");

	// Held keys and mouse buttons must never remain stuck after focus changes.
	SendMessageW(handle, WM_SETFOCUS, 0, 0);
	SendMessageW(handle, WM_KEYDOWN, 'W', KeyData(0x11));
	SendMessageW(handle, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(12, 25));
	Require(Input::Get().GetMouseState(nullptr, nullptr) & MOUSE_BUTTON_LMASK, "Mouse button state missing");
	const auto buttonEvents = DrainEvents();
	Require(std::any_of(buttonEvents.begin(), buttonEvents.end(), [](const Event& event)
		{ return event.mType == EventType::MouseButton && event.mPressed && event.mMouseX == 12 && event.mMouseY == 25; }),
		"Mouse button event lost its client coordinates");
	SendMessageW(handle, WM_KILLFOCUS, 0, 0);
	Require(!Input::Get().IsKeyPressed(Input::KEYCODE_W), "Focus loss did not clear held keys");
	Require(Input::Get().GetMouseState(nullptr, nullptr) == 0, "Focus loss did not clear held mouse buttons");
	DrainEvents();

	SendMessageW(handle, WM_CHAR, 0x0627, 0);
	SendMessageW(handle, WM_CHAR, 0xd83d, 0);
	SendMessageW(handle, WM_CHAR, 0xde00, 0);
	SendMessageW(handle, WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA), 0);
	SendMessageW(handle, WM_MOUSEHWHEEL, MAKEWPARAM(0, -WHEEL_DELTA), 0);
	const auto textEvents = DrainEvents();
	Require(std::any_of(textEvents.begin(), textEvents.end(), [](const Event& event)
		{ return event.mType == EventType::TextInput && event.mCodepoint == 0x0627; }), "BMP text input failed");
	Require(std::any_of(textEvents.begin(), textEvents.end(), [](const Event& event)
		{ return event.mType == EventType::TextInput && event.mCodepoint == 0x1f600; }), "Surrogate-pair text input failed");
	Require(std::any_of(textEvents.begin(), textEvents.end(), [](const Event& event)
		{ return event.mType == EventType::MouseWheel && event.mWheelY == 1.0f; }), "Vertical wheel input failed");
	Require(std::any_of(textEvents.begin(), textEvents.end(), [](const Event& event)
		{ return event.mType == EventType::MouseWheel && event.mWheelX == -1.0f; }), "Horizontal wheel input failed");

	Input::Get().SetMouseInputMode(Input::MouseInputMode::Locked);
	Require(Input::Get().GetMouseInputMode() == Input::MouseInputMode::Locked, "Mouse lock mode was not retained");
	Input::Get().SetMouseInputMode(Input::MouseInputMode::Normal);
	SendMessageW(handle, WM_CLOSE, 0, 0);
	Require(IsWindow(handle), "Closing destroyed the window before resource teardown");
	const auto closeEvents = DrainEvents();
	Require(std::any_of(closeEvents.begin(), closeEvents.end(), [](const Event& event)
		{ return event.mType == EventType::WindowClose; }), "Window close event missing");

	window.Destroy();
	window.Destroy();
	Require(!IsWindow(handle) && !window.GetNativeHandle(), "Window destruction failed");
	Require(DrainEvents().empty(), "Destroyed window left dangling queued events");
	Require(window.GetAspectRatioF32() == 0.0f, "Empty window aspect ratio should be zero");
	Shutdown();
	Shutdown();
	Require(Initialize(), "Platform did not restart after shutdown");
	desc.WindowWidth = 0;
	Require(!window.Create(desc), "Zero-sized window should be rejected");
	Shutdown();
}

int main()
{
	try
	{
		RunChecks();
		std::cout << "Nuclear.Platform Windows smoke checks passed.\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		Shutdown();
		std::cerr << error.what() << "\n" << Nuclear::Platform::GetLastError() << "\n";
		return 1;
	}
}
