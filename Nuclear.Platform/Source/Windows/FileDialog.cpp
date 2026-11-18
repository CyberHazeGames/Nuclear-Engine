#include <Nuclear.Platform/FileDialog.h>
#include "Win32Platform.h"
#include <commdlg.h>
#include <vector>

namespace Nuclear::Platform
{
	static std::filesystem::path SelectFile(Window& owner, const std::string& title,
		const std::string& patterns, const std::filesystem::path& directory, const std::string& extension, bool save, std::string& error)
	{
		error.clear();
		const auto handle = static_cast<HWND>(owner.GetNativeHandle());
		if (!handle || !IsWindow(handle))
		{
			error = "File selection requires a live owner window.";
			return {};
		}

		const auto nativeTitle = Detail::ToWide(title);
		const auto nativeDirectory = directory.wstring();
		std::wstring filter = L"Supported files";
		filter.push_back(L'\0');
		filter += Detail::ToWide(patterns);
		filter.push_back(L'\0');
		filter += L"All files (*.*)";
		filter.push_back(L'\0');
		filter += L"*.*";
		filter.append(2, L'\0');
		std::vector<wchar_t> filename(32768, L'\0');

		OPENFILENAMEW options{};
		options.lStructSize = sizeof(options);
		options.hwndOwner = handle;
		options.lpstrTitle = nativeTitle.c_str();
		options.lpstrFilter = filter.c_str();
		options.nFilterIndex = 1;
		options.lpstrInitialDir = nativeDirectory.c_str();
		options.lpstrFile = filename.data();
		options.nMaxFile = static_cast<DWORD>(filename.size());
		const auto defaultExtension = Detail::ToWide(extension);
		options.lpstrDefExt = save ? defaultExtension.c_str() : nullptr;
		options.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR |
			(save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
		if (save ? GetSaveFileNameW(&options) : GetOpenFileNameW(&options))
			return std::filesystem::path(filename.data());

		const DWORD code = CommDlgExtendedError();
		if (code != 0)
			error = "File dialog failed (Windows error " + std::to_string(code) + ").";
		return {};
	}

	std::filesystem::path FileDialog::OpenFile(Window& owner, const std::string& title,
		const std::string& patterns, const std::filesystem::path& directory, std::string& error)
	{
		return SelectFile(owner, title, patterns, directory, {}, false, error);
	}

	std::filesystem::path FileDialog::SaveFile(Window& owner, const std::string& title,
		const std::string& patterns, const std::filesystem::path& directory, const std::string& extension,
		std::string& error)
	{
		return SelectFile(owner, title, patterns, directory, extension, true, error);
	}
}
