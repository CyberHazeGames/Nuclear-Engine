#include <UILayers/LoggerView.h>
#include <fstream>
namespace Nuclear::Editor
{
	std::string LoggerView::Read() const
	{
		std::ifstream stream("NuclearEngine.log", std::ios::binary | std::ios::ate);
		if (!stream)
			return "No session log is available.";
		const auto size = static_cast<std::streamoff>(stream.tellg());
		const auto start = std::max<std::streamoff>(0, size - 16384);
		stream.seekg(start);
		std::string result(static_cast<size_t>(size - start), '\0');
		stream.read(result.data(), static_cast<std::streamsize>(result.size()));
		if (start != 0)
			result.erase(0, result.find('\n') == std::string::npos ? result.size() : result.find('\n') + 1);
		return result;
	}
} // namespace Nuclear::Editor
