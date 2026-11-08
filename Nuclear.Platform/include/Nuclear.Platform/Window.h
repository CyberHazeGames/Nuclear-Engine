#pragma once

#include <Nuclear.Platform/Export.h>
#include <cstdint>
#include <functional>
#include <string>

namespace Nuclear::Platform
{
	/// Initial client-area dimensions and appearance of a native window.
	struct WindowDesc
	{
		std::uint32_t WindowWidth = 1024; ///< Client width in pixels; must be positive.
		std::uint32_t WindowHeight = 768; ///< Client height in pixels; must be positive.
		bool FullScreen = false; ///< Create a borderless window covering the primary monitor.
		bool CustomTitleBar = false; ///< Extend the client area over the frame; caller paints its own caption.
		std::string Title = "NuclearEngine"; ///< Window title encoded as UTF-8.
	};

	/// Owns one native window. Create, use, and destroy it on the platform event thread.
	/// Initialize the platform first and destroy all windows before platform shutdown.
	class NPAPI Window
	{
	public:
		/// Constructs an empty window without allocating native resources.
		Window() = default;
		/// Destroys any remaining native window.
		~Window();

		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;

		/// Creates and shows the window; returns false on failure (see GetLastError()).
		bool Create(const WindowDesc& desc);
		/// Releases the native window. Calling this repeatedly is safe.
		void Destroy();
		/// Shows or hides the window without destroying it.
		void Display(bool show = true);

		/// Writes the current client dimensions in pixels, or zero for an empty window.
		void GetSize(std::uint32_t& width, std::uint32_t& height);
		/// Signed overload of GetSize, for native and UI consumers.
		void GetSize(int& width, int& height);
		/// Returns client width / height, or zero when minimized or empty.
		float GetAspectRatioF32();

		/// Resizes the client area in pixels; zero dimensions are ignored.
		void SetSize(std::uint32_t width, std::uint32_t height);
		/// Replaces the UTF-8 window title.
		void SetTitle(const std::string& title);
		/// Returns a borrowed native handle (HWND on Windows), valid until Destroy().
		void* GetNativeHandle() const;
		/// Returns whether this window currently receives keyboard input.
		bool HasFocus() const;
		/// Converts client pixel coordinates to desktop coordinates; false if the window is not live.
		bool ClientToScreen(int& x, int& y) const;
		/// Converts desktop pixel coordinates to this window's client pixels; false if not live.
		bool ScreenToClient(int& x, int& y) const;
		/// Installs an event-thread caption hit test in client pixels. Clear before its captures expire.
		void SetTitleBarHitTest(std::function<bool(int, int)> hitTest);
		/// True when custom chrome is enabled and the callback identifies a draggable caption point.
		bool IsTitleBarAt(int x, int y) const;
		/// Whether this live window uses client-drawn chrome (fullscreen windows do not).
		bool HasCustomTitleBar() const { return mCustomTitleBar; }
		/// Minimizes a live window without changing ownership.
		void Minimize();
		/// Switches a live window between maximized and restored placement.
		void ToggleMaximize();
		/// True while the native window is maximized.
		bool IsMaximized() const;
		/// Queues the normal WindowClose event; leaves destruction to the owning client.
		void RequestClose();

	private:
		void* pWindow = nullptr; ///< Owned native handle, hidden from portable headers.
		bool mCustomTitleBar = false; ///< Opt-in chrome policy, set before native creation callbacks.
		std::function<bool(int, int)> mTitleBarHitTest; ///< Borrowed UI callback, cleared on destruction.
	};
}
