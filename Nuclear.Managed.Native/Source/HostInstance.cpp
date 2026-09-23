#include "Nuclear/Managed/HostInstance.hpp"
#include "Nuclear/Managed/StringHelper.hpp"
#include "Nuclear/Managed/TypeCache.hpp"

#include "Verify.hpp"
#include "HostFXRErrorCodes.hpp"
#include "HostFXRDiscovery.hpp"
#include "ManagedFunctions.hpp"

#ifdef NUCLEAR_MANAGED_WINDOWS
	#include <ShlObj_core.h>
#else
	#include <dlfcn.h>
#endif

namespace Nuclear::Managed {

	struct CoreCLRFunctions
	{
		hostfxr_set_error_writer_fn SetHostFXRErrorWriter = nullptr;
		hostfxr_set_runtime_property_value_fn SetRuntimePropertyValue = nullptr;
		hostfxr_initialize_for_runtime_config_fn InitHostFXRForRuntimeConfig = nullptr;
		hostfxr_get_runtime_delegate_fn GetRuntimeDelegate = nullptr;
		hostfxr_close_fn CloseHostFXR = nullptr;
		load_assembly_and_get_function_pointer_fn GetManagedFunctionPtr = nullptr;
	};
	static CoreCLRFunctions s_CoreCLRFunctions;

	static MessageCallbackFn MessageCallback = nullptr;
	static MessageLevel MessageFilter;
	static ExceptionCallbackFn ExceptionCallback = nullptr;

	static void DefaultMessageCallback(std::string_view InMessage, MessageLevel InLevel)
	{
		const char* level = "";

		switch (InLevel)
		{
		default: break;
		case MessageLevel::Trace:
			level = "Trace";
			break;
		case MessageLevel::Info:
			level = "Info";
			break;
		case MessageLevel::Warning:
			level = "Warn";
			break;
		case MessageLevel::Error:
			level = "Error";
			break;
		}

		std::cout << "[Nuclear.Managed](" << level << "): " << InMessage << std::endl;
	}

	HostInitStatus HostInstance::Initialize(HostSettings InSettings)
	{
		NUCLEAR_MANAGED_VERIFY(!m_Initialized);

		if (!LoadHostFXR())
		{
			return HostInitStatus::DotNetNotFound;
		}

		// Setup settings
		m_Settings = std::move(InSettings);

		if (!m_Settings.MessageCallback)
			m_Settings.MessageCallback = DefaultMessageCallback;
		MessageCallback = m_Settings.MessageCallback;
		MessageFilter = m_Settings.MessageFilter;

		s_CoreCLRFunctions.SetHostFXRErrorWriter([](const UCChar* InMessage)
		{
			auto message = StringHelper::ConvertWideToUtf8(InMessage);
			MessageCallback(message, MessageLevel::Error);
		});

		m_ManagedBridgeAssemblyPath = std::filesystem::path(m_Settings.RuntimeDirectory) / "Nuclear.Managed.dll";

		if (!std::filesystem::exists(m_ManagedBridgeAssemblyPath))
		{
			MessageCallback("Failed to find Nuclear.Managed.dll", MessageLevel::Error);
			return HostInitStatus::ManagedBridgeNotFound;
		}

		if (!InitializeManagedBridge())
		{
			return HostInitStatus::ManagedBridgeInitError;
		}

		return HostInitStatus::Success;
	}

	void HostInstance::Shutdown()
	{
		s_CoreCLRFunctions.CloseHostFXR(m_HostFXRContext);
	}
	
	AssemblyLoadContext HostInstance::CreateAssemblyLoadContext(std::string_view InName)
	{
		ScopedString name = String::New(InName);
		ScopedString dllPath = String::New("");
		AssemblyLoadContext alc;
		alc.m_ContextId = s_ManagedFunctions.CreateAssemblyLoadContextFptr(name, dllPath);
		alc.m_Host = this;
		return alc;
	}

	AssemblyLoadContext HostInstance::CreateAssemblyLoadContext(std::string_view InName, std::string_view InDllPath)
	{
		ScopedString name = String::New(InName);
		ScopedString dllPath = String::New(InDllPath);
		AssemblyLoadContext alc;
		alc.m_ContextId = s_ManagedFunctions.CreateAssemblyLoadContextFptr(name, dllPath);
		alc.m_Host = this;
		return alc;
	}

	void HostInstance::UnloadAssemblyLoadContext(AssemblyLoadContext& InLoadContext)
	{
		s_ManagedFunctions.UnloadAssemblyLoadContextFptr(InLoadContext.m_ContextId);
		InLoadContext.m_ContextId = -1;
		InLoadContext.m_LoadedAssemblies.Clear();
	}

#ifdef NUCLEAR_MANAGED_WINDOWS
	template <typename TFunc>
	TFunc LoadFunctionPtr(void* InLibraryHandle, const char* InFunctionName)
	{
		auto result = (TFunc)GetProcAddress((HMODULE)InLibraryHandle, InFunctionName);
		NUCLEAR_MANAGED_VERIFY(result);
		return result;
	}
#else
	template <typename TFunc>
	TFunc LoadFunctionPtr(void* InLibraryHandle, const char* InFunctionName)
	{
		auto result = (TFunc)dlsym(InLibraryHandle, InFunctionName);
		NUCLEAR_MANAGED_VERIFY(result);
		return result;
	}
#endif

	static std::filesystem::path GetHostFXRPath()
	{
#ifdef NUCLEAR_MANAGED_WINDOWS
		std::filesystem::path basePath = "";
		
		// Find the Program Files folder
		TCHAR pf[MAX_PATH];
		SHGetSpecialFolderPath(
		nullptr,
		pf,
		CSIDL_PROGRAM_FILES,
		FALSE);

		basePath = pf;
		basePath /= "dotnet/host/fxr/";

		auto searchPaths = std::array
		{
			basePath
		};
#elif defined(NUCLEAR_MANAGED_APPLE)
		auto searchPaths = std::array
		{
			std::filesystem::path("/usr/local/share/dotnet/host/fxr/"),
			std::filesystem::path("/usr/share/dotnet/host/fxr/")
		};
#else
		auto searchPaths = std::array
		{
			std::filesystem::path("/usr/local/lib/dotnet/host/fxr/"),
			std::filesystem::path("/usr/local/lib64/dotnet/host/fxr/"),
			std::filesystem::path("/usr/local/share/dotnet/host/fxr/"),

			std::filesystem::path("/usr/lib/dotnet/host/fxr/"),
			std::filesystem::path("/usr/lib64/dotnet/host/fxr/"),
			std::filesystem::path("/usr/share/dotnet/host/fxr/")
		};
#endif

		for (const auto& path : searchPaths)
		{
			auto host = Detail::FindLatestHostFXR(path, NUCLEAR_MANAGED_HOSTFXR_NAME);
			if (!host.empty())
				return host;
		}

		return "";
	}

	bool HostInstance::LoadHostFXR() const
	{
		// Retrieve the file path to the CoreCLR library
		auto hostfxrPath = GetHostFXRPath();

		if (hostfxrPath.empty())
		{
			return false;
		}

		// Load the CoreCLR library
		void* libraryHandle = nullptr;

#ifdef NUCLEAR_MANAGED_WINDOWS
	#ifdef NUCLEAR_MANAGED_WIDE_CHARS
		libraryHandle = LoadLibraryW(hostfxrPath.c_str());
	#else
		libraryHandle = LoadLibraryA(hostfxrPath.string().c_str());
	#endif
#else
		libraryHandle = dlopen(hostfxrPath.string().data(), RTLD_NOW | RTLD_GLOBAL);
#endif

		if (libraryHandle == nullptr)
		{
			return false;
		}

		// Load CoreCLR functions
		s_CoreCLRFunctions.SetHostFXRErrorWriter = LoadFunctionPtr<hostfxr_set_error_writer_fn>(libraryHandle, "hostfxr_set_error_writer");
		s_CoreCLRFunctions.SetRuntimePropertyValue = LoadFunctionPtr<hostfxr_set_runtime_property_value_fn>(libraryHandle, "hostfxr_set_runtime_property_value");
		s_CoreCLRFunctions.InitHostFXRForRuntimeConfig = LoadFunctionPtr<hostfxr_initialize_for_runtime_config_fn>(libraryHandle, "hostfxr_initialize_for_runtime_config");
		s_CoreCLRFunctions.GetRuntimeDelegate = LoadFunctionPtr<hostfxr_get_runtime_delegate_fn>(libraryHandle, "hostfxr_get_runtime_delegate");
		s_CoreCLRFunctions.CloseHostFXR = LoadFunctionPtr<hostfxr_close_fn>(libraryHandle, "hostfxr_close");

		return true;
	}
	
	bool HostInstance::InitializeManagedBridge()
	{
		// Fetch load_assembly_and_get_function_pointer_fn from CoreCLR
		{
			auto runtimeConfigPath = std::filesystem::path(m_Settings.RuntimeDirectory) / "Nuclear.Managed.runtimeconfig.json";

			if (!std::filesystem::exists(runtimeConfigPath))
			{
				MessageCallback("Failed to find Nuclear.Managed.runtimeconfig.json", MessageLevel::Error);
				return false;
			}

			int status = s_CoreCLRFunctions.InitHostFXRForRuntimeConfig(runtimeConfigPath.c_str(), nullptr, &m_HostFXRContext);
			NUCLEAR_MANAGED_VERIFY(status == StatusCode::Success || status == StatusCode::Success_HostAlreadyInitialized || status == StatusCode::Success_DifferentRuntimeProperties);
			NUCLEAR_MANAGED_VERIFY(m_HostFXRContext != nullptr);

			std::filesystem::path runtimeDirectoryPath = m_Settings.RuntimeDirectory;
			s_CoreCLRFunctions.SetRuntimePropertyValue(m_HostFXRContext, NUCLEAR_MANAGED_STR("APP_CONTEXT_BASE_DIRECTORY"), runtimeDirectoryPath.c_str());

			status = s_CoreCLRFunctions.GetRuntimeDelegate(m_HostFXRContext, hdt_load_assembly_and_get_function_pointer, (void**) &s_CoreCLRFunctions.GetManagedFunctionPtr);
			NUCLEAR_MANAGED_VERIFY(status == StatusCode::Success);
		}

		using InitializeFn = void(*)(void(*)(String, MessageLevel), void(*)(String));
		InitializeFn managedBridgeEntryPoint = nullptr;
		managedBridgeEntryPoint = LoadManagedBridgeFunctionPtr<InitializeFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.ManagedHost, Nuclear.Managed"), NUCLEAR_MANAGED_STR("Initialize"));

		LoadManagedFunctions();

		managedBridgeEntryPoint([](String InMessage, MessageLevel InLevel)
		{
			if (MessageFilter & InLevel)
			{
				std::string message = InMessage;
				MessageCallback(message, InLevel);
			}
		},
		[](String InMessage)
		{
			std::string message = InMessage;
			if (!ExceptionCallback)
			{
				MessageCallback(message, MessageLevel::Error);
				return;
			}
			
			ExceptionCallback(message);
		});

		ExceptionCallback = m_Settings.ExceptionCallback;

		return true;
	}

	void HostInstance::LoadManagedFunctions()
	{
		s_ManagedFunctions.CreateAssemblyLoadContextFptr = LoadManagedBridgeFunctionPtr<CreateAssemblyLoadContextFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.AssemblyLoader, Nuclear.Managed"), NUCLEAR_MANAGED_STR("CreateAssemblyLoadContext"));
		s_ManagedFunctions.UnloadAssemblyLoadContextFptr = LoadManagedBridgeFunctionPtr<UnloadAssemblyLoadContextFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.AssemblyLoader, Nuclear.Managed"), NUCLEAR_MANAGED_STR("UnloadAssemblyLoadContext"));
		s_ManagedFunctions.LoadAssemblyFptr = LoadManagedBridgeFunctionPtr<LoadAssemblyFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.AssemblyLoader, Nuclear.Managed"), NUCLEAR_MANAGED_STR("LoadAssembly"));
		s_ManagedFunctions.LoadAssemblyFromMemoryFptr = LoadManagedBridgeFunctionPtr<LoadAssemblyFromMemoryFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.AssemblyLoader, Nuclear.Managed"), NUCLEAR_MANAGED_STR("LoadAssemblyFromMemory"));
		s_ManagedFunctions.UnloadAssemblyLoadContextFptr = LoadManagedBridgeFunctionPtr<UnloadAssemblyLoadContextFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.AssemblyLoader, Nuclear.Managed"), NUCLEAR_MANAGED_STR("UnloadAssemblyLoadContext"));
		s_ManagedFunctions.GetLastLoadStatusFptr = LoadManagedBridgeFunctionPtr<GetLastLoadStatusFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.AssemblyLoader, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetLastLoadStatus"));
		s_ManagedFunctions.GetAssemblyNameFptr = LoadManagedBridgeFunctionPtr<GetAssemblyNameFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.AssemblyLoader, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetAssemblyName"));

		s_ManagedFunctions.RunMSBuildFptr = LoadManagedBridgeFunctionPtr<RunMSBuildFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.MSBuildRunner, Nuclear.Managed"), NUCLEAR_MANAGED_STR("Run"));

		s_ManagedFunctions.GetAssemblyTypesFptr = LoadManagedBridgeFunctionPtr<GetAssemblyTypesFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetAssemblyTypes"));
		s_ManagedFunctions.GetFullTypeNameFptr = LoadManagedBridgeFunctionPtr<GetFullTypeNameFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetFullTypeName"));
		s_ManagedFunctions.GetAssemblyQualifiedNameFptr = LoadManagedBridgeFunctionPtr<GetAssemblyQualifiedNameFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetAssemblyQualifiedName"));
		s_ManagedFunctions.GetBaseTypeFptr = LoadManagedBridgeFunctionPtr<GetBaseTypeFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetBaseType"));
		s_ManagedFunctions.GetInterfaceTypeCountFptr = LoadManagedBridgeFunctionPtr<GetInterfaceTypeCountFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetInterfaceTypeCount"));
		s_ManagedFunctions.GetInterfaceTypesFptr = LoadManagedBridgeFunctionPtr<GetInterfaceTypesFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetInterfaceTypes"));
		s_ManagedFunctions.GetTypeSizeFptr = LoadManagedBridgeFunctionPtr<GetTypeSizeFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetTypeSize"));
		s_ManagedFunctions.IsTypeSubclassOfFptr = LoadManagedBridgeFunctionPtr<IsTypeSubclassOfFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("IsTypeSubclassOf"));
		s_ManagedFunctions.IsTypeAssignableToFptr = LoadManagedBridgeFunctionPtr<IsTypeAssignableToFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("IsTypeAssignableTo"));
		s_ManagedFunctions.IsTypeAssignableFromFptr = LoadManagedBridgeFunctionPtr<IsTypeAssignableFromFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("IsTypeAssignableFrom"));
		s_ManagedFunctions.IsTypeSZArrayFptr = LoadManagedBridgeFunctionPtr<IsTypeSZArrayFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("IsTypeSZArray"));
		s_ManagedFunctions.GetElementTypeFptr = LoadManagedBridgeFunctionPtr<GetElementTypeFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetElementType"));
		s_ManagedFunctions.GetTypeMethodsFptr = LoadManagedBridgeFunctionPtr<GetTypeMethodsFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetTypeMethods"));
		s_ManagedFunctions.GetTypeFieldsFptr = LoadManagedBridgeFunctionPtr<GetTypeFieldsFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetTypeFields"));
		s_ManagedFunctions.GetTypePropertiesFptr = LoadManagedBridgeFunctionPtr<GetTypePropertiesFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetTypeProperties"));
		s_ManagedFunctions.HasTypeAttributeFptr = LoadManagedBridgeFunctionPtr<HasTypeAttributeFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("HasTypeAttribute"));
		s_ManagedFunctions.GetTypeAttributesFptr = LoadManagedBridgeFunctionPtr<GetTypeAttributesFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetTypeAttributes"));
		s_ManagedFunctions.GetTypeManagedTypeFptr = LoadManagedBridgeFunctionPtr<GetTypeManagedTypeFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetTypeManagedType"));
		s_ManagedFunctions.InvokeStaticMethodFptr = LoadManagedBridgeFunctionPtr<InvokeStaticMethodFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.ManagedObject, Nuclear.Managed"), NUCLEAR_MANAGED_STR("InvokeStaticMethod"));
		s_ManagedFunctions.InvokeStaticMethodRetFptr = LoadManagedBridgeFunctionPtr<InvokeStaticMethodRetFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.ManagedObject, Nuclear.Managed"), NUCLEAR_MANAGED_STR("InvokeStaticMethodRet"));

		s_ManagedFunctions.GetMethodInfoNameFptr = LoadManagedBridgeFunctionPtr<GetMethodInfoNameFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetMethodInfoName"));
		s_ManagedFunctions.GetMethodInfoReturnTypeFptr = LoadManagedBridgeFunctionPtr<GetMethodInfoReturnTypeFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetMethodInfoReturnType"));
		s_ManagedFunctions.GetMethodInfoParameterTypesFptr = LoadManagedBridgeFunctionPtr<GetMethodInfoParameterTypesFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetMethodInfoParameterTypes"));
		s_ManagedFunctions.GetMethodInfoAccessibilityFptr = LoadManagedBridgeFunctionPtr<GetMethodInfoAccessibilityFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetMethodInfoAccessibility"));
		s_ManagedFunctions.GetMethodInfoAttributesFptr = LoadManagedBridgeFunctionPtr<GetMethodInfoAttributesFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetMethodInfoAttributes"));

		s_ManagedFunctions.GetFieldInfoNameFptr = LoadManagedBridgeFunctionPtr<GetFieldInfoNameFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetFieldInfoName"));
		s_ManagedFunctions.GetFieldInfoTypeFptr = LoadManagedBridgeFunctionPtr<GetFieldInfoTypeFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetFieldInfoType"));
		s_ManagedFunctions.GetFieldInfoAccessibilityFptr = LoadManagedBridgeFunctionPtr<GetFieldInfoAccessibilityFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetFieldInfoAccessibility"));
		s_ManagedFunctions.GetFieldInfoAttributesFptr = LoadManagedBridgeFunctionPtr<GetFieldInfoAttributesFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetFieldInfoAttributes"));

		s_ManagedFunctions.GetPropertyInfoNameFptr = LoadManagedBridgeFunctionPtr<GetPropertyInfoNameFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetPropertyInfoName"));
		s_ManagedFunctions.GetPropertyInfoTypeFptr = LoadManagedBridgeFunctionPtr<GetPropertyInfoTypeFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetPropertyInfoType"));
		s_ManagedFunctions.GetPropertyInfoAttributesFptr = LoadManagedBridgeFunctionPtr<GetPropertyInfoAttributesFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetPropertyInfoAttributes"));

		s_ManagedFunctions.GetAttributeFieldValueFptr = LoadManagedBridgeFunctionPtr<GetAttributeFieldValueFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetAttributeFieldValue"));
		s_ManagedFunctions.GetAttributeTypeFptr = LoadManagedBridgeFunctionPtr<GetAttributeTypeFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.TypeInterface, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetAttributeType"));

		s_ManagedFunctions.SetInternalCallsFptr = LoadManagedBridgeFunctionPtr<SetInternalCallsFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.Interop.InternalCallsManager, Nuclear.Managed"), NUCLEAR_MANAGED_STR("SetInternalCalls"));
		s_ManagedFunctions.CreateObjectFptr = LoadManagedBridgeFunctionPtr<CreateObjectFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.ManagedObject, Nuclear.Managed"), NUCLEAR_MANAGED_STR("CreateObject"));
		s_ManagedFunctions.CopyObjectFptr = LoadManagedBridgeFunctionPtr<CopyObjectFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.ManagedObject, Nuclear.Managed"), NUCLEAR_MANAGED_STR("CopyObject"));
		s_ManagedFunctions.InvokeMethodFptr = LoadManagedBridgeFunctionPtr<InvokeMethodFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.ManagedObject, Nuclear.Managed"), NUCLEAR_MANAGED_STR("InvokeMethod"));
		s_ManagedFunctions.InvokeMethodRetFptr = LoadManagedBridgeFunctionPtr<InvokeMethodRetFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.ManagedObject, Nuclear.Managed"), NUCLEAR_MANAGED_STR("InvokeMethodRet"));
		s_ManagedFunctions.SetFieldValueFptr = LoadManagedBridgeFunctionPtr<SetFieldValueFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.ManagedObject, Nuclear.Managed"), NUCLEAR_MANAGED_STR("SetFieldValue"));
		s_ManagedFunctions.GetFieldValueFptr = LoadManagedBridgeFunctionPtr<GetFieldValueFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.ManagedObject, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetFieldValue"));
		s_ManagedFunctions.SetPropertyValueFptr = LoadManagedBridgeFunctionPtr<SetFieldValueFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.ManagedObject, Nuclear.Managed"), NUCLEAR_MANAGED_STR("SetPropertyValue"));
		s_ManagedFunctions.GetPropertyValueFptr = LoadManagedBridgeFunctionPtr<GetFieldValueFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.ManagedObject, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetPropertyValue"));
		s_ManagedFunctions.DestroyObjectFptr = LoadManagedBridgeFunctionPtr<DestroyObjectFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.ManagedObject, Nuclear.Managed"), NUCLEAR_MANAGED_STR("DestroyObject"));
		s_ManagedFunctions.GetObjectTypeIdFptr = LoadManagedBridgeFunctionPtr<GetObjectTypeIdFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.ManagedObject, Nuclear.Managed"), NUCLEAR_MANAGED_STR("GetObjectTypeId"));

		s_ManagedFunctions.CollectGarbageFptr = LoadManagedBridgeFunctionPtr<CollectGarbageFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.GarbageCollector, Nuclear.Managed"), NUCLEAR_MANAGED_STR("CollectGarbage"));
		s_ManagedFunctions.WaitForPendingFinalizersFptr = LoadManagedBridgeFunctionPtr<WaitForPendingFinalizersFn>(NUCLEAR_MANAGED_STR("Nuclear.Managed.GarbageCollector, Nuclear.Managed"), NUCLEAR_MANAGED_STR("WaitForPendingFinalizers"));
	}

	void* HostInstance::LoadManagedBridgeFunctionPtr(const std::filesystem::path& InAssemblyPath, const UCChar* InTypeName, const UCChar* InMethodName, const UCChar* InDelegateType) const
	{
		void* funcPtr = nullptr;

		int status = s_CoreCLRFunctions.GetManagedFunctionPtr(InAssemblyPath.c_str(), InTypeName, InMethodName, InDelegateType, nullptr, &funcPtr);
		if(status != StatusCode::Success || !funcPtr) {
			std::cerr << "Failed to retrieve managed function pointer `" << InTypeName << "`::`" << InMethodName << "` from `" << InAssemblyPath << "`" << std::endl;
			NUCLEAR_MANAGED_VERIFY(false);
		}

		return funcPtr;
	}
}
