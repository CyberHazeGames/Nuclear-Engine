#include <Scripting/ScriptingModule.h>
#include <Utilities/Logger.h>
#include <algorithm>

namespace Nuclear::Scripting
{
	ScriptingModule& ScriptingModule::Get() { static ScriptingModule instance; return instance; }
	bool ScriptingModule::OnInitialize() { return Initialize(mStartupDesc); }
	ScriptingModule::~ScriptingModule() { Shutdown(); }

	bool ScriptingModule::RegisterBackend(std::unique_ptr<IScriptingBackend> backend)
	{
		if (!backend || backend->GetName().empty() || pActiveBackend) return false;
		std::string name(backend->GetName());
		return mBackends.emplace(std::move(name), std::move(backend)).second;
	}

	bool ScriptingModule::UnregisterBackend(std::string_view name)
	{
		if (pActiveBackend) return false;
		return mBackends.erase(std::string(name)) != 0;
	}

	IScriptingBackend* ScriptingModule::FindBackend(std::string_view name) const
	{
		auto found = mBackends.find(std::string(name));
		return found == mBackends.end() ? nullptr : found->second.get();
	}

	std::vector<std::string> ScriptingModule::GetBackendNames() const
	{
		std::vector<std::string> names;
		for (const auto& [name, backend] : mBackends) names.push_back(name);
		std::sort(names.begin(), names.end());
		return names;
	}

	bool ScriptingModule::Initialize(const ScriptingModuleDesc& desc)
	{
		if (pActiveBackend) return false;
		auto backend = FindBackend(desc.mBackendName);
		if (!backend)
		{
			NUCLEAR_ERROR("[ScriptingModule] Backend not registered: {0}", desc.mBackendName);
			return false;
		}
		if (!backend->Initialize()) return false;
		pActiveBackend = backend;
		return true;
	}

	void ScriptingModule::Shutdown()
	{
		if (!pActiveBackend) return;
		pActiveBackend->Shutdown();
		pActiveBackend = nullptr;
	}

	bool ScriptingModule::IsInitialized() const { return pActiveBackend && pActiveBackend->IsInitialized(); }

	bool ScriptingModule::CreateScriptAsset(Assets::Script* script, const std::string& scriptclassname)
	{
		if (!script || !IsInitialized()) return false;
		auto type = pActiveBackend->FindClass(scriptclassname);
		if (!type || !pActiveBackend->IsEntityClass(type))
		{
			NUCLEAR_ERROR("[ScriptingModule] Script class is missing or is not an entity: {0}", scriptclassname);
			script->mClass = {};
			return false;
		}
		script->mClass.pClass = std::move(type);
		script->mOnStartMethod = "OnStart";
		script->mOnUpdateMethod = "OnUpdate";
		return true;
	}

	const std::string& ScriptingModule::GetClientNamespace() const
	{
		static const std::string empty;
		return pActiveBackend ? pActiveBackend->GetClientNamespace() : empty;
	}
}
