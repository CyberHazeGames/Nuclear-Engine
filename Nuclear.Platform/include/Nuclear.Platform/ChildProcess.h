#pragma once
#include <Nuclear.Platform/Export.h>
#include <filesystem>
#include <optional>
#include <vector>

namespace Nuclear::Platform
{
	/// One owned Windows child. Destruction stops a remaining child and releases its process handle.
	class NPAPI ChildProcess
	{
	public:
		ChildProcess() = default;
		ChildProcess(const ChildProcess&) = delete;
		ChildProcess& operator=(const ChildProcess&) = delete;
		~ChildProcess();
		/// Starts a program with literal arguments and an independent working directory. Throws on failure.
		void Start(const std::filesystem::path& executable, const std::vector<std::filesystem::path>& arguments,
			const std::filesystem::path& directory);
		/// Returns the completed exit code, or empty while running.
		std::optional<unsigned long> ExitCode() const;
		/// Requests graceful window closure. Safe to repeat while the child is starting.
		void RequestClose() const;
	private:
		void* mHandle = nullptr; ///< Owned until destruction.
		unsigned long mId = 0; ///< Used only while the process is live.
	};
}
