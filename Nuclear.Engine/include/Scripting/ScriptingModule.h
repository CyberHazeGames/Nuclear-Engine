#pragma once
#include <Core/EngineModule.h>
#include <Scripting/ScriptingAssembly.h>
#include <Scripting/ScriptingClass.h>
#include <Scripting/ScriptingObject.h>
#include <Scripting/ScriptingRegistry.h>
#include <Assets/Script.h>
#include <Core/Path.h>
#include <memory>

namespace Nuclear
{
	namespace Scripting
	{
		struct ScriptingAssemblyCreationDesc
		{
			std::string mNamespaceName;
			Core::Path mPath;
		};
		struct ScriptingModuleDesc
		{
			// Directory containing ScriptCore and the Nuclear.Managed runtime files.
			Core::Path mScriptingCoreAssemblyDir;
			std::string mClientNamespace = "ClientScripts";
			Core::Path mClientAssemblyPath;
			bool mAutoInitClientAssembly = true;
		};
		class NEAPI ScriptingModule : public Core::EngineModule<ScriptingModule>
		{
			friend class Core::EngineModule<ScriptingModule>;
			friend class ScriptingClass;
		public:
			~ScriptingModule();
			bool Initialize(const ScriptingModuleDesc& desc);
			void Shutdown() override;
			bool IsInitialized() const;
			bool CreateScriptAsset(Assets::Script* script, const std::string& scriptclassname);
			ScriptingClass CreateScriptClass(ScriptingAssembly* assembly, const ScriptingClassCreationDesc& desc);
			bool CreateScriptingAssembly(ScriptingAssembly* assembly, const ScriptingAssemblyCreationDesc& desc);
			ScriptingAssembly* GetCoreAssembly();
			ScriptingAssembly* GetClientAssembly();
			ScriptingRegistry& GetRegistry();
		private:
			ScriptingModule();
			void InitBindings();
			void TrackObject(const std::shared_ptr<Nuclear::Managed::ManagedObject>& object);
			struct Runtime;
			std::unique_ptr<Runtime> pRuntime;
			ScriptingAssembly mCoreAssembly;
			ScriptingAssembly mClientAssembly;
			ScriptingRegistry mRegistry;
		};
	}
}
