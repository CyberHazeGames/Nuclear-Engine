#pragma once

#include "Core.hpp"
#include "MessageLevel.hpp"
#include "Assembly.hpp"
#include "ManagedObject.hpp"

#include <functional>

namespace Nuclear::Managed {

	using ExceptionCallbackFn = std::function<void(std::string_view)>;

	struct HostSettings
	{
		/// <summary>
		/// Directory containing Nuclear.Managed.dll and Nuclear.Managed.runtimeconfig.json.
		/// </summary>
		std::string RuntimeDirectory;

		MessageCallbackFn MessageCallback = nullptr;
		MessageLevel MessageFilter = MessageLevel::All;

		ExceptionCallbackFn ExceptionCallback = nullptr;
	};

	enum class HostInitStatus
	{
		Success,
		ManagedBridgeNotFound,
		ManagedBridgeInitError,
		DotNetNotFound,
	};

	class HostInstance
	{
	public:
		HostInitStatus Initialize(HostSettings InSettings);
		void Shutdown();

		AssemblyLoadContext CreateAssemblyLoadContext(std::string_view InName);
		void UnloadAssemblyLoadContext(AssemblyLoadContext& InLoadContext);

		// `InDllPath` is a colon-separated list of paths from which AssemblyLoader will try and resolve load paths at runtime.
		// This does not affect the behaviour of LoadAssembly from native code.
		AssemblyLoadContext CreateAssemblyLoadContext(std::string_view InName, std::string_view InDllPath);

	private:
		bool LoadHostFXR() const;
		bool InitializeManagedBridge();
		void LoadManagedFunctions();

		void* LoadManagedBridgeFunctionPtr(const std::filesystem::path& InAssemblyPath, const UCChar* InTypeName, const UCChar* InMethodName, const UCChar* InDelegateType = NUCLEAR_MANAGED_UNMANAGED_CALLERS_ONLY) const;

		template<typename TFunc>
		TFunc LoadManagedBridgeFunctionPtr(const UCChar* InTypeName, const UCChar* InMethodName, const UCChar* InDelegateType = NUCLEAR_MANAGED_UNMANAGED_CALLERS_ONLY) const
		{
			return (TFunc) LoadManagedBridgeFunctionPtr(m_ManagedBridgeAssemblyPath, InTypeName, InMethodName, InDelegateType);
		}

	private:
		HostSettings m_Settings;
		std::filesystem::path m_ManagedBridgeAssemblyPath;
		void* m_HostFXRContext = nullptr;
		bool m_Initialized = false;

		friend class AssemblyLoadContext;
	};

}
