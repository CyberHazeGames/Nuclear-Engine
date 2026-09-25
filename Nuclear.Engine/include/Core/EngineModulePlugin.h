#pragma once
#include <cstdint>

namespace Nuclear::Core
{
	class EngineModule;

	inline constexpr std::uint32_t EngineModulePluginVersion = 1;
	inline constexpr const char* EngineModulePluginEntryPoint = "GetNuclearEngineModulePlugin";

	// Strings and arrays belong to the DLL and are copied by ModuleManager before registration.
	struct PluginModuleFactory
	{
		const char* pName;
		std::uint32_t mType;
		const char* const* pDependencies;
		std::uint32_t mDependencyCount;
		EngineModule* (*pCreate)();
		void (*pDestroy)(EngineModule*);
	};

	struct EngineModulePlugin
	{
		std::uint32_t mSize;
		std::uint32_t mVersion;
		const PluginModuleFactory* pModules;
		std::uint32_t mModuleCount;
	};

	using GetEngineModulePlugin = const EngineModulePlugin* (*)();
}
