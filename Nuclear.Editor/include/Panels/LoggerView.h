#pragma once
#include <string>
namespace Nuclear::Editor
{
	/// Reads a bounded tail of the current session log without owning or modifying the file.
	class LoggerView
	{
	  public:
		std::string Read() const; ///< Returns at most 16 KiB, discarding an incomplete first line.
	};
} // namespace Nuclear::Editor
