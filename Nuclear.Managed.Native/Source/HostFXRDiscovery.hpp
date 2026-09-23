#pragma once

#include <array>
#include <charconv>
#include <filesystem>
#include <optional>
#include <string_view>

namespace Nuclear::Managed::Detail {

	// Only stable major.minor.patch directories are candidates for this host.
	inline std::optional<std::array<unsigned int, 3>> ParseHostFXRVersion(std::string_view name)
	{
		std::array<unsigned int, 3> version{};
		for (size_t index = 0; index < version.size(); ++index)
		{
			const auto separator = name.find('.');
			const auto part = name.substr(0, separator);
			if (part.empty())
				return std::nullopt;
			const auto result = std::from_chars(part.data(), part.data() + part.size(), version[index]);
			if (result.ec != std::errc{} || result.ptr != part.data() + part.size())
				return std::nullopt;
			if (index == version.size() - 1)
			{
				if (separator != std::string_view::npos)
					return std::nullopt;
			}
			else
			{
				if (separator == std::string_view::npos)
					return std::nullopt;
				name.remove_prefix(separator + 1);
			}
		}
		return version[0] == 10 ? std::optional(version) : std::nullopt;
	}

	inline std::filesystem::path FindLatestHostFXR(const std::filesystem::path& directory, const char* libraryName)
	{
		std::filesystem::path selected;
		std::array<unsigned int, 3> latest{};
		if (!std::filesystem::is_directory(directory))
			return selected;
		for (const auto& entry : std::filesystem::directory_iterator(directory))
		{
			if (!entry.is_directory())
				continue;
			const auto version = ParseHostFXRVersion(entry.path().filename().string());
			const auto library = entry.path() / libraryName;
			if (version && *version > latest && std::filesystem::is_regular_file(library))
			{
				latest = *version;
				selected = library;
			}
		}
		return selected;
	}
}
