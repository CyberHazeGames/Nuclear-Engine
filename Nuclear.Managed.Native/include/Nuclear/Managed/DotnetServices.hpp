#pragma once

#include <string>

namespace Nuclear::Managed
{
	class DotnetServices
	{
	public:
		static bool RunMSBuild(const std::string& InSolutionPath, bool InBuildDebug = true);
	};
}
