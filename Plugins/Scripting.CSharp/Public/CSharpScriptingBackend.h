#pragma once
#include <Scripting/IScriptingBackend.h>
#include <Core/Path.h>
#include "ScriptingAssembly.h"
#include "ScriptingRegistry.h"
#include <memory>
#include <vector>

namespace Nuclear::Managed { class ManagedObject; }

namespace Nuclear::Scripting::CSharp
{
	struct CSharpBackendDesc
	{
		Core::Path mScriptingCoreAssemblyDir;
		std::string mClientNamespace = "ClientScripts";
		Core::Path mClientAssemblyPath;
		bool mAutoInitClientAssembly = true;
	};

	class CSharpScriptingBackend final : public IScriptingBackend
	{
	public:
		explicit CSharpScriptingBackend(CSharpBackendDesc desc);
		~CSharpScriptingBackend() override;
		std::string_view GetName() const override { return "CSharp"; }
		bool Initialize() override;
		void Shutdown() override;
		bool IsInitialized() const override;
		std::shared_ptr<IScriptClass> FindClass(const std::string& fullName) override;
		bool IsEntityClass(const std::shared_ptr<IScriptClass>& type) override;
		const std::string& GetClientNamespace() const override;
		ScriptingRegistry& GetRegistry() { return mRegistry; }
		void TrackObject(const std::shared_ptr<Nuclear::Managed::ManagedObject>& object);
	private:
		bool CreateScriptingAssembly(ScriptingAssembly* assembly, const std::string& name, const Core::Path& path);
		void InitBindings();
		struct Runtime;
		std::unique_ptr<Runtime> pRuntime;
		ScriptingAssembly mCoreAssembly;
		ScriptingAssembly mClientAssembly;
		ScriptingRegistry mRegistry;
		CSharpBackendDesc mDesc;
	};

	// Called by the application composition root before ScriptingModule::Initialize.
	NEAPI bool RegisterCSharpBackend(CSharpBackendDesc desc);
}
