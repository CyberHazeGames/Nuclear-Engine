#pragma once

#include <Nuclear.Platform/Event.h>
#include <Nuclear.Platform/Networking.h>
#include <Nuclear.Platform/Window.h>
#include <filesystem>
#include <string>

namespace Nuclear::Platform
{
	/// Available system cursor shapes. Hidden/locked input modes override the shape.
	enum class MouseCursor
	{
		Arrow,
		TextInput,
		ResizeAll,
		ResizeNS,
		ResizeEW,
		ResizeNESW,
		ResizeNWSE,
		Hand,
		NotAllowed,
		Hidden
	};

	/// Initializes the native backend on the calling thread. Repeated calls are safe.
	NPAPI bool Initialize();
	/// Releases backend state after all windows are destroyed, on the initializing thread.
	NPAPI void Shutdown();
	/// Pumps native messages and returns the next queued event, or false if none remain.
	/// Dispatch every event before starting the next frame. Use only on the event thread.
	NPAPI bool PollEvent(Event& event);
	/// Returns the last backend failure as UTF-8; successful calls need not clear it.
	NPAPI std::string GetLastError();

	/// Reads UTF-8 clipboard text, returning an empty string if unavailable.
	NPAPI std::string GetClipboardText();
	/// Copies UTF-8 text to the clipboard; returns false on failure.
	NPAPI bool SetClipboardText(const std::string& text);
	/// Sets the system cursor shape for the focused platform window.
	NPAPI void SetMouseCursor(MouseCursor cursor);
	/// Moves the cursor to a client-relative pixel position in the supplied window.
	NPAPI void SetMousePosition(Window& window, int x, int y);

	/// Returns the absolute executable filename, or an empty path on failure.
	NPAPI std::filesystem::path GetExecutablePath();
	/// Loads an absolute library path and its dependencies. The caller must unload it.
	/// Native library handles and symbols are opaque outside the platform backend.
	NPAPI void* LoadDynamicLibrary(const std::filesystem::path& path);
	/// Returns a borrowed exported symbol address, or null when missing.
	NPAPI void* GetDynamicLibrarySymbol(void* library, const char* name);
	/// Releases a library after all of its objects and function pointers are discarded.
	NPAPI void UnloadDynamicLibrary(void* library);
}
