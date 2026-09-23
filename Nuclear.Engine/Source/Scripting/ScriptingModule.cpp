#include "ManagedRuntime.h"
#include <Scripting/ScriptingModule.h>
#include <Scripting/ScriptingBindings.h>
#include <Utilities/Logger.h>
#include <algorithm>

namespace Nuclear
{
	namespace Scripting
	{
		struct ScriptingModule::Runtime
		{
			Nuclear::Managed::HostInstance mHost;
			Nuclear::Managed::AssemblyLoadContext mContext;
			std::vector<std::weak_ptr<Nuclear::Managed::ManagedObject>> mObjects;
			std::shared_ptr<void> mLifetime;
			bool mInitialized = false;
		};

		ScriptingModule::ScriptingModule() : pRuntime(std::make_unique<Runtime>()) {}
		ScriptingModule::~ScriptingModule() = default;

		bool ScriptingModule::Initialize(const ScriptingModuleDesc& desc)
		{
			if (IsInitialized())
				return false;
			auto directory = std::filesystem::absolute(desc.mScriptingCoreAssemblyDir.GetRealPath());
			for (const auto* filename : { "Nuclear.Managed.dll", "Nuclear.Managed.runtimeconfig.json", "Nuclear.ScriptCore.dll" })
			{
				if (!std::filesystem::is_regular_file(directory / filename))
				{
					NUCLEAR_ERROR("[ScriptingModule] Missing runtime file: {0}", (directory / filename).string());
					return false;
				}
			}
			Nuclear::Managed::HostSettings settings;
			settings.RuntimeDirectory = directory.string();
			settings.MessageCallback = [](std::string_view message, Nuclear::Managed::MessageLevel level) {
				if (level == Nuclear::Managed::MessageLevel::Error)
					NUCLEAR_ERROR("[.NET] {0}", message);
				else if (level == Nuclear::Managed::MessageLevel::Warning)
					NUCLEAR_WARN("[.NET] {0}", message);
				else
					NUCLEAR_TRACE("[.NET] {0}", message);
			};
			settings.ExceptionCallback = [](std::string_view message) {
				NUCLEAR_ERROR("[.NET] Managed exception: {0}", message);
			};
			auto status = pRuntime->mHost.Initialize(settings);
			if (status != Nuclear::Managed::HostInitStatus::Success)
			{
				NUCLEAR_ERROR("[ScriptingModule] Nuclear.Managed initialization failed ({0}); install the x64 .NET 10 runtime.", static_cast<int>(status));
				return false;
			}
			pRuntime->mContext = pRuntime->mHost.CreateAssemblyLoadContext("Nuclear.Scripts");
			pRuntime->mLifetime = std::make_shared<int>(0);
			pRuntime->mInitialized = true;
			ScriptingAssemblyCreationDesc core;
			core.mNamespaceName = "Nuclear";
			core.mPath = (directory / "Nuclear.ScriptCore.dll").string();
			if (!CreateScriptingAssembly(&mCoreAssembly, core))
			{
				Shutdown();
				return false;
			}
			InitBindings();
			mRegistry.RegisterEngineComponents(&mCoreAssembly);
			if (desc.mAutoInitClientAssembly)
			{
				ScriptingAssemblyCreationDesc client;
				client.mNamespaceName = desc.mClientNamespace;
				client.mPath = desc.mClientAssemblyPath;
				if (!CreateScriptingAssembly(&mClientAssembly, client))
				{
					Shutdown();
					return false;
				}
			}
			NUCLEAR_INFO("[ScriptingModule] .NET scripting initialized through Nuclear.Managed.");
			return true;
		}
		void ScriptingModule::Shutdown()
		{
			if (!IsInitialized())
				return;
			// Components may outlive the runtime. Release every managed handle first.
			for (auto& weak : pRuntime->mObjects)
				if (auto object = weak.lock())
					object->Destroy();
			pRuntime->mObjects.clear();
			mRegistry.Clear();
			mCoreAssembly = {};
			mClientAssembly = {};
			pRuntime->mLifetime.reset();
			pRuntime->mHost.UnloadAssemblyLoadContext(pRuntime->mContext);
			pRuntime->mHost.Shutdown();
			pRuntime->mInitialized = false;
		}
		bool ScriptingModule::IsInitialized() const { return pRuntime->mInitialized; }
		void ScriptingModule::TrackObject(const std::shared_ptr<Nuclear::Managed::ManagedObject>& object)
		{
			std::erase_if(pRuntime->mObjects, [](const auto& weak) { return weak.expired(); });
			pRuntime->mObjects.push_back(object);
		}
		bool ScriptingModule::CreateScriptAsset(Assets::Script* script, const std::string& scriptclassname)
		{
			if (!script || !IsInitialized())
				return false;
			script->mClass = CreateScriptClass(&mClientAssembly, ScriptingClassCreationDesc(scriptclassname));
			auto type = script->mClass.GetClassPtr();
			auto& entityType = mCoreAssembly.GetAssembly()->GetLocalType("Nuclear.ECS.Entity");
			if (!type || !type->IsSubclassOf(entityType))
			{
				NUCLEAR_ERROR("[ScriptingModule] Script must derive from Nuclear.ECS.Entity: {0}", scriptclassname);
				script->mClass = {};
				return false;
			}
			script->mOnStartMethod = "OnStart";
			script->mOnUpdateMethod = "OnUpdate";
			return true;
		}
		ScriptingClass ScriptingModule::CreateScriptClass(ScriptingAssembly* assembly, const ScriptingClassCreationDesc& desc)
		{
			ScriptingClass result;
			if (!IsInitialized() || !assembly || !assembly->GetAssembly())
				return result;
			result.mDesc = desc;
			result.mRuntimeLifetime = pRuntime->mLifetime;
			auto name = desc.mNamespaceName.empty() ? desc.mClassName : desc.mNamespaceName + "." + desc.mClassName;
			auto& type = assembly->GetAssembly()->GetLocalType(name);
			if (type)
				result.pClass = &type;
			else
				NUCLEAR_ERROR("[ScriptingModule] Managed class not found: {0}", name);
			return result;
		}
		bool ScriptingModule::CreateScriptingAssembly(ScriptingAssembly* assembly, const ScriptingAssemblyCreationDesc& desc)
		{
			if (!IsInitialized() || !assembly)
				return false;
			auto path = std::filesystem::absolute(desc.mPath.GetRealPath()).string();
			auto& loaded = pRuntime->mContext.LoadAssembly(path);
			if (loaded.GetLoadStatus() != Nuclear::Managed::AssemblyLoadStatus::Success)
			{
				NUCLEAR_ERROR("[ScriptingModule] Failed to load assembly: {0}", path);
				return false;
			}
			assembly->pAssembly = &loaded;
			assembly->mNamespaceName = desc.mNamespaceName;
			return true;
		}
		ScriptingAssembly* ScriptingModule::GetCoreAssembly() { return &mCoreAssembly; }
		ScriptingAssembly* ScriptingModule::GetClientAssembly() { return &mClientAssembly; }
		ScriptingRegistry& ScriptingModule::GetRegistry() { return mRegistry; }

		void ScriptingModule::InitBindings()
		{
			auto assembly = mCoreAssembly.GetAssembly();
#define REGISTER_CALL(Name, Function) assembly->AddInternalCall("Nuclear.NativeCalls", Name, reinterpret_cast<void*>(&Bindings::Function))
			REGISTER_CALL("LoggerTrace", Utilities_Logger_Trace);
			REGISTER_CALL("LoggerInfo", Utilities_Logger_Info);
			REGISTER_CALL("LoggerWarn", Utilities_Logger_Warn);
			REGISTER_CALL("LoggerError", Utilities_Logger_Error);
			REGISTER_CALL("LoggerFatal", Utilities_Logger_FatalError);
			REGISTER_CALL("AddComponent", ECS_Entity_AddComponent);
			REGISTER_CALL("HasComponent", ECS_Entity_HasComponent);
			REGISTER_CALL("EntityGetName", Components_EntityInfoComponent_GetName);
			REGISTER_CALL("EntitySetName", Components_EntityInfoComponent_SetName);
			REGISTER_CALL("IsKeyPressed", Platform_Input_IsKeyPressed);
			REGISTER_CALL("LightGetColor", Components_LightComponent_GetColor);
			REGISTER_CALL("LightSetColor", Components_LightComponent_SetColor);
			REGISTER_CALL("LightGetIntensity", Components_LightComponent_GetIntensity);
			REGISTER_CALL("LightSetIntensity", Components_LightComponent_SetIntensity);
#undef REGISTER_CALL
			assembly->UploadInternalCalls();
		}
	}
}
