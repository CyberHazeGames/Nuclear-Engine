#pragma once

#include <Nuclear.Platform/Export.h>
#include <cstdint>
#include <string>

namespace Nuclear::Platform
{
	/// Initial client-area dimensions and appearance of a native window.
	struct WindowDesc
	{
		std::uint32_t WindowWidth = 1024; ///< Client width in pixels; must be positive.
		std::uint32_t WindowHeight = 768; ///< Client height in pixels; must be positive.
		bool FullScreen = false; ///< Create a borderless window covering the primary monitor.
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

	private:
		void* pWindow = nullptr; ///< Owned native handle, hidden from portable headers.
	};
}
