#pragma once
#include <NE_Common.h>
#include <memory>
#include <string>

namespace Nuclear::Scripting
{
	class IScriptObject;
	class ScriptingClass;

	class NEAPI ScriptingObject
	{
	public:
		bool IsValid() const;
		void CallMethod(const std::string& method);
		void CallMethod(const std::string& method, float value);
		ScriptingClass* GetScriptingClass();
	private:
		friend class ScriptingClass;
		std::shared_ptr<IScriptObject> pObject;
		ScriptingClass* pParent = nullptr;
	};
}
