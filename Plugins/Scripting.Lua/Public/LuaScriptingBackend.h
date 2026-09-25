#pragma once
#include <Scripting/IScriptingBackend.h>

namespace Nuclear::Scripting::Lua
{
	// Placeholder for a future Lua runtime. Applications must not register it yet.
	class LuaScriptingBackend final : public IScriptingBackend
	{
	public:
		std::string_view GetName() const override { return "Lua"; }
		bool Initialize() override { return false; }
		void Shutdown() override {}
		bool IsInitialized() const override { return false; }
		std::shared_ptr<IScriptClass> FindClass(const std::string&) override { return {}; }
		bool IsEntityClass(const std::shared_ptr<IScriptClass>&) override { return false; }
		const std::string& GetClientNamespace() const override { return mNamespace; }
	private:
		std::string mNamespace;
	};
}
