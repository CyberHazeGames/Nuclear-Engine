#include <filesystem>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <stdexcept>
#include <Nuclear/Managed/HostInstance.hpp>
#include <Nuclear/Managed/GC.hpp>
#include "../../Nuclear.Managed.Native/Source/HostFXRDiscovery.hpp"

namespace
{
	struct Color { float r, g, b, a; };
	std::string Name;
	Color LightColor{};
	float Intensity = 0;
	Nuclear::Managed::TypeId InfoType = -1, LightType = -1;
	bool HasLight = false;
	int Logs = 0, Exceptions = 0, Errors = 0;
	std::string LastException;

	void Require(bool condition, const char* message)
	{
		if (!condition) throw std::runtime_error(message);
	}
	void Log(Nuclear::Managed::String text) { ++Logs; std::cout << std::string(text) << '\n'; }
	uint32_t Add(uint32_t id, Nuclear::Managed::ReflectionType type)
	{
		if (id != 42 || (type.m_TypeID != InfoType && type.m_TypeID != LightType)) return 0;
		if (type.m_TypeID == LightType) HasLight = true;
		return 1;
	}
	uint32_t Has(uint32_t id, Nuclear::Managed::ReflectionType type)
	{
		return id == 42 && (type.m_TypeID == InfoType || (type.m_TypeID == LightType && HasLight));
	}
	Nuclear::Managed::String GetName(uint32_t) { return Nuclear::Managed::String::New(Name); }
	void SetName(uint32_t, Nuclear::Managed::String name) { Name = std::string(name); }
	uint32_t Key(int32_t key) { return key == 10; }
	void GetColor(uint32_t, Color* value) { *value = LightColor; }
	void SetColor(uint32_t, Color* value) { LightColor = *value; }
	float GetIntensity(uint32_t) { return Intensity; }
	void SetIntensity(uint32_t, float value) { Intensity = value; }

	void Register(Nuclear::Managed::ManagedAssembly& assembly)
	{
#define BIND(Name, Function) assembly.AddInternalCall("Nuclear.NativeCalls", Name, reinterpret_cast<void*>(&Function))
		BIND("LoggerTrace", Log); BIND("LoggerInfo", Log); BIND("LoggerWarn", Log);
		BIND("LoggerError", Log); BIND("LoggerFatal", Log);
		BIND("AddComponent", Add); BIND("HasComponent", Has);
		BIND("EntityGetName", GetName); BIND("EntitySetName", SetName);
		BIND("IsKeyPressed", Key);
		BIND("LightGetColor", GetColor); BIND("LightSetColor", SetColor);
		BIND("LightGetIntensity", GetIntensity); BIND("LightSetIntensity", SetIntensity);
#undef BIND
		assembly.UploadInternalCalls();
	}
}

int main(int argc, char** argv)
{
	try
	{
		using Nuclear::Managed::Detail::ParseHostFXRVersion;
		Require(ParseHostFXRVersion("10.0.12").has_value(), "Stable .NET 10 host was rejected.");
		Require(*ParseHostFXRVersion("10.0.12") > *ParseHostFXRVersion("10.0.9"), "Host patch ordering must be numeric.");
		for (const auto* version : { "9.0.20", "100.0.1", "10", "10.0", "10..1", "10.0.1.2", "10.0.1-preview.1", "10.0.-1", "10.0.999999999999", "" })
			Require(!ParseHostFXRVersion(version), "Unsupported host version was accepted.");
		auto directory = std::filesystem::absolute(argc > 1 ? argv[1] : "Bin/DebugX64");
		Nuclear::Managed::HostSettings settings;
		settings.RuntimeDirectory = directory.string();
		settings.MessageCallback = [](std::string_view message, Nuclear::Managed::MessageLevel level) {
			if (level == Nuclear::Managed::MessageLevel::Error) { ++Errors; std::cerr << message << '\n'; }
		};
		settings.ExceptionCallback = [](std::string_view message) { ++Exceptions; LastException = message; };
		Nuclear::Managed::HostInstance host;
		Require(host.Initialize(settings) == Nuclear::Managed::HostInitStatus::Success, "Host initialization failed.");
		for (int pass = 0; pass < 2; ++pass)
		{
			auto context = host.CreateAssemblyLoadContext("Smoke");
			auto& core = context.LoadAssembly((directory / "Nuclear.ScriptCore.dll").string());
			Require(core.GetLoadStatus() == Nuclear::Managed::AssemblyLoadStatus::Success, "ScriptCore load failed.");
			Register(core);
			InfoType = core.GetLocalType("Nuclear.Components.EntityInfoComponent").GetTypeId();
			LightType = core.GetLocalType("Nuclear.Components.LightComponent").GetTypeId();
			HasLight = false;
			auto& assembly = context.LoadAssembly((directory / "ScriptingSmoke.Managed.dll").string());
			Require(assembly.GetLoadStatus() == Nuclear::Managed::AssemblyLoadStatus::Success, "Probe assembly load failed.");
			Require(!assembly.GetLocalType("ScriptingSmoke.Missing"), "Missing type unexpectedly resolved.");
			{
				auto object = assembly.GetLocalType("ScriptingSmoke.Probe").CreateInstance();
				Require(object.IsValid(), "Script construction failed.");
				Require(object.GetFieldValue<int>("RuntimeMajor") == 10, "Expected the .NET 10 runtime.");
				Require(object.GetFieldValue<int>("BridgeIdentityMatches") == 1, "Expected Nuclear.Managed assembly identity.");
				object.InvokeMethod("BindEntity", uint32_t(42));
				object.InvokeMethod("OnStart");
				Nuclear::Managed::GC::Collect();
				Nuclear::Managed::GC::WaitForPendingFinalizers();
				object.InvokeMethod("OnUpdate", 0.125f);
				Require(object.GetFieldValue<uint32_t>("StartedWithID") == 42, "ID was not set before OnStart.");
				Require(object.GetFieldValue<float>("LastDelta") == 0.125f, "Update argument failed.");
				Require(Exceptions == pass && Errors == 0, "Unexpected managed exception or binding error.");
				object.InvokeMethod("ThrowExpected");
				Require(Exceptions == pass + 1 && LastException.find("Expected smoke exception") != std::string::npos,
					"Managed exception did not reach the host.");
				object.Destroy();
				Require(!object.IsValid(), "Managed handle was not released.");
			}
			host.UnloadAssemblyLoadContext(context);
		}
		Require(Logs == 10, "Logger bindings were not all called.");
		host.Shutdown();
		std::cout << "PASS: Nuclear.Managed on .NET 10, scripting ABI, callbacks, GC, exceptions, and context reload.\n";
		return 0;
	}
	catch (const std::exception& exception)
	{
		std::cerr << "FAIL: " << exception.what() << '\n';
		return 1;
	}
}
