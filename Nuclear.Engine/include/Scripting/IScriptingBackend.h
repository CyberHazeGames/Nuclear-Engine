#pragma once
#include <NE_Common.h>
#include <memory>
#include <string>
#include <string_view>

namespace Nuclear::Scripting
{
	class IScriptObject
	{
	public:
		virtual ~IScriptObject() = default;
		virtual bool IsValid() const = 0;
		virtual void CallMethod(const std::string& method) = 0;
		virtual void CallMethod(const std::string& method, float value) = 0;
	};

	class IScriptClass
	{
	public:
		virtual ~IScriptClass() = default;
		virtual std::shared_ptr<IScriptObject> CreateObject(Uint32 entityID) = 0;
	};

	struct ScriptingModuleDesc
	{
		std::string mBackendName = "CSharp";
	};

	class IScriptingBackend
	{
	public:
		virtual ~IScriptingBackend() = default;
		virtual std::string_view GetName() const = 0;
		virtual bool Initialize() = 0;
		virtual void Shutdown() = 0;
		virtual bool IsInitialized() const = 0;
		virtual std::shared_ptr<IScriptClass> FindClass(const std::string& fullName) = 0;
		virtual bool IsEntityClass(const std::shared_ptr<IScriptClass>& type) = 0;
		virtual const std::string& GetClientNamespace() const = 0;
	};
}
