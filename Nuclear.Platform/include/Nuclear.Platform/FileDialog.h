#pragma once

#include <Nuclear.Platform/Export.h>
#include <filesystem>
#include <string>

namespace Nuclear::Platform
{
	class Window;

	/// Native modal file selection on the window/event thread. No engine dependencies.
	class NPAPI FileDialog
	{
	public:
		/// Opens a single-file picker owned by a live window without changing the working directory.
		/// Title and semicolon-separated wildcard patterns are UTF-8; directory is the initial location.
		/// Returns the selected absolute path, or empty on cancellation/failure. Error is cleared on
		/// success/cancellation and receives diagnostic text on failure. Owner must outlive the call.
		static std::filesystem::path OpenFile(Window& owner, const std::string& title,
			const std::string& patterns, const std::filesystem::path& directory, std::string& error);
	};
}
