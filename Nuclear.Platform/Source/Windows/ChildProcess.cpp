#include <Nuclear.Platform/ChildProcess.h>
#include "Win32Platform.h"
#include <system_error>

namespace Nuclear::Platform
{
	namespace
	{
		std::wstring Quote(const std::wstring& value)
		{
			std::wstring result = L"\"";
			size_t slashes = 0;
			for (auto c : value)
			{
				if (c == L'\\') { ++slashes; continue; }
				result.append(c == L'"' ? 2 * slashes + 1 : slashes, L'\\');
				result.push_back(c);
				slashes = 0;
			}
			result.append(2 * slashes, L'\\');
			return result + L'"';
		}
	}
	void ChildProcess::Start(const std::filesystem::path& executable,
		const std::vector<std::filesystem::path>& arguments, const std::filesystem::path& directory)
	{
		if (mHandle) throw std::logic_error("Child already started");
		std::wstring command = Quote(executable.wstring());
		for (const auto& argument : arguments) command += L" " + Quote(argument.wstring());
		STARTUPINFOW startup{};
		startup.cb = sizeof(startup);
		PROCESS_INFORMATION process{};
		if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
			nullptr, directory.c_str(), &startup, &process))
			throw std::system_error(::GetLastError(), std::system_category(), "Start game");
		CloseHandle(process.hThread);
		mHandle = process.hProcess;
		mId = process.dwProcessId;
	}
	std::optional<unsigned long> ChildProcess::ExitCode() const
	{
		DWORD code = 0;
		if (!mHandle || WaitForSingleObject(mHandle, 0) != WAIT_OBJECT_0) return {};
		if (!GetExitCodeProcess(mHandle, &code)) throw std::system_error(::GetLastError(), std::system_category());
		return code;
	}
	void ChildProcess::RequestClose() const
	{
		if (!mHandle || ExitCode()) return;
		EnumWindows([](HWND window, LPARAM id) -> BOOL {
			DWORD owner = 0;
			GetWindowThreadProcessId(window, &owner);
			if (owner == static_cast<DWORD>(id)) PostMessageW(window, WM_CLOSE, 0, 0);
			return TRUE;
		}, mId);
	}
	ChildProcess::~ChildProcess()
	{
		if (!mHandle) return;
		if (WaitForSingleObject(mHandle, 0) == WAIT_TIMEOUT)
		{
			RequestClose();
			if (WaitForSingleObject(mHandle, 1500) == WAIT_TIMEOUT)
			{
				TerminateProcess(mHandle, 1);
				WaitForSingleObject(mHandle, 1500);
			}
		}
		CloseHandle(mHandle);
	}
}
