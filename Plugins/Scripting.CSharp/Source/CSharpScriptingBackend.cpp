#include "ManagedRuntime.h"
#include "../Public/CSharpScriptingBackend.h"
#include <Scripting/ScriptingModule.h>
#include "../Public/ScriptingBindings.h"
#include <Utilities/Logger.h>
#include <Nuclear/Managed/Type.hpp>
#include <algorithm>
#include <utility>

namespace Nuclear
{
	namespace Scripting::CSharp
	{
		class CSharpScriptObject final : public IScriptObject
		{
		public:
			explicit CSharpScriptObject(std::shared_ptr<Nuclear::Managed::ManagedObject> object) : pObject(std::move(object)) {}
			bool IsValid() const override { return pObject && pObject->IsValid(); }
			void CallMethod(const std::string& method) override { if (IsValid()) pObject->InvokeMethod(method); }
			void CallMethod(const std::string& method, float value) override { if (IsValid()) pObject->InvokeMethod(method, value); }
		private:
			std::shared_ptr<Nuclear::Managed::ManagedObject> pObject;
		};

		class CSharpScriptClass final : public IScriptClass
		{
		public:
			CSharpScriptClass(Nuclear::Managed::Type* type, std::weak_ptr<void> lifetime,
				CSharpScriptingBackend& backend, std::string name)
				: pType(type), mLifetime(std::move(lifetime)), mBackend(backend), mName(std::move(name)) {}
			std::shared_ptr<IScriptObject> CreateObject(Uint32 entityID) override
			{
				if (mLifetime.expired() || !pType || !*pType) return {};
				auto object = std::make_shared<Nuclear::Managed::ManagedObject>(pType->CreateInstance());
				if (!object->IsValid())
				{
					NUCLEAR_ERROR("[CSharp] Failed to construct {0}", mName);
					return {};
				}
				mBackend.TrackObject(object);
				object->InvokeMethod("BindEntity", entityID);
				return std::make_shared<CSharpScriptObject>(std::move(object));
			}
			Nuclear::Managed::Type* GetType() const { return mLifetime.expired() ? nullptr : pType; }
		private:
			Nuclear::Managed::Type* pType;
			std::weak_ptr<void> mLifetime;
			CSharpScriptingBackend& mBackend;
			std::string mName;
		};

		struct CSharpScriptingBackend::Runtime
		{
			Nuclear::Managed::HostInstance mHost;
			Nuclear::Managed::AssemblyLoadContext mContext;
			std::vector<std::weak_ptr<Nuclear::Managed::ManagedObject>> mObjects;
			std::shared_ptr<void> mLifetime;
			bool mInitialized = false;
		};

		CSharpScriptingBackend::CSharpScriptingBackend(CSharpBackendDesc desc)
			: pRuntime(std::make_unique<Runtime>()), mDesc(std::move(desc)) {}
		CSharpScriptingBackend::~CSharpScriptingBackend() = default;

		bool CSharpScriptingBackend::Initialize()
		{
			if (IsInitialized())
				return false;
			auto directory = std::filesystem::absolute(mDesc.mScriptingCoreAssemblyDir.GetRealPath());
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
			if (!CreateScriptingAssembly(&mCoreAssembly, "Nuclear", (directory / "Nuclear.ScriptCore.dll").string()))
			{
				Shutdown();
				return false;
			}
			InitBindings();
			mRegistry.RegisterEngineComponents(&mCoreAssembly);
			if (mDesc.mAutoInitClientAssembly)
			{
				if (!CreateScriptingAssembly(&mClientAssembly, mDesc.mClientNamespace, mDesc.mClientAssemblyPath))
				{
					Shutdown();
					return false;
				}
			}
			NUCLEAR_INFO("[ScriptingModule] .NET scripting initialized through Nuclear.Managed.");
			return true;
		}
		void CSharpScriptingBackend::Shutdown()
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
		bool CSharpScriptingBackend::IsInitialized() const { return pRuntime->mInitialized; }
		void CSharpScriptingBackend::TrackObject(const std::shared_ptr<Nuclear::Managed::ManagedObject>& object)
		{
			std::erase_if(pRuntime->mObjects, [](const auto& weak) { return weak.expired(); });
			pRuntime->mObjects.push_back(object);
		}
		std::shared_ptr<IScriptClass> CSharpScriptingBackend::FindClass(const std::string& fullName)
		{
			if (!IsInitialized() || !mClientAssembly.GetAssembly()) return {};
			auto& type = mClientAssembly.GetAssembly()->GetLocalType(fullName);
			if (!type)
			{
				NUCLEAR_ERROR("[CSharp] Managed class not found: {0}", fullName);
				return {};
			}
			return std::make_shared<CSharpScriptClass>(&type, pRuntime->mLifetime, *this, fullName);
		}
		bool CSharpScriptingBackend::IsEntityClass(const std::shared_ptr<IScriptClass>& type)
		{
			if (!IsInitialized() || !type || !mCoreAssembly.GetAssembly()) return false;
			auto csharpType = std::dynamic_pointer_cast<CSharpScriptClass>(type);
			auto& entityType = mCoreAssembly.GetAssembly()->GetLocalType("Nuclear.ECS.Entity");
			return csharpType && csharpType->GetType() && csharpType->GetType()->IsSubclassOf(entityType);
		}
		bool CSharpScriptingBackend::CreateScriptingAssembly(ScriptingAssembly* assembly, const std::string& name, const Core::Path& assemblyPath)
		{
			if (!IsInitialized() || !assembly)
				return false;
			auto path = std::filesystem::absolute(assemblyPath.GetRealPath()).string();
			auto& loaded = pRuntime->mContext.LoadAssembly(path);
			if (loaded.GetLoadStatus() != Nuclear::Managed::AssemblyLoadStatus::Success)
			{
				NUCLEAR_ERROR("[ScriptingModule] Failed to load assembly: {0}", path);
				return false;
			}
			assembly->pAssembly = &loaded;
			assembly->mNamespaceName = name;
			return true;
		}
		const std::string& CSharpScriptingBackend::GetClientNamespace() const { return mClientAssembly.GetNamespaceName(); }

		void CSharpScriptingBackend::InitBindings()
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

		bool RegisterCSharpBackend(CSharpBackendDesc desc)
		{
			auto& scripting = ScriptingModule::Get();
			if (scripting.FindBackend("CSharp") && !scripting.UnregisterBackend("CSharp")) return false;
			return scripting.RegisterBackend(std::make_unique<CSharpScriptingBackend>(std::move(desc)));
		}
	}
}
