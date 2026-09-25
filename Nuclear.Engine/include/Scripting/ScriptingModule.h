#pragma once
#include <Core/EngineModule.h>
#include <Scripting/IScriptingBackend.h>
#include <Assets/Script.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Nuclear::Scripting
{
	class NEAPI ScriptingModule : public Core::EngineModule
	{
	public:
		static ScriptingModule& Get();
		void SetStartupDesc(const ScriptingModuleDesc& desc) { mStartupDesc = desc; }
		bool OnInitialize() override;
		~ScriptingModule();
		bool RegisterBackend(std::unique_ptr<IScriptingBackend> backend);
		bool UnregisterBackend(std::string_view name);
		IScriptingBackend* FindBackend(std::string_view name) const;
		std::vector<std::string> GetBackendNames() const;
		bool Initialize(const ScriptingModuleDesc& desc);
		void Shutdown() override;
		bool IsInitialized() const;
		bool CreateScriptAsset(Assets::Script* script, const std::string& scriptclassname);
		const std::string& GetClientNamespace() const;
	private:
		ScriptingModule() = default;
		ScriptingModuleDesc mStartupDesc{};
		std::unordered_map<std::string, std::unique_ptr<IScriptingBackend>> mBackends;
		IScriptingBackend* pActiveBackend = nullptr;
	};
}
