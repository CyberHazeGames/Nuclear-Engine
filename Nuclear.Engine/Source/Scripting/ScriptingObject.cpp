#include <Scripting/ScriptingObject.h>
#include <Scripting/IScriptingBackend.h>

namespace Nuclear::Scripting
{
	bool ScriptingObject::IsValid() const { return pObject && pObject->IsValid(); }
	void ScriptingObject::CallMethod(const std::string& method)
	{
		if (IsValid() && !method.empty()) pObject->CallMethod(method);
	}
	void ScriptingObject::CallMethod(const std::string& method, float value)
	{
		if (IsValid() && !method.empty()) pObject->CallMethod(method, value);
	}
	ScriptingClass* ScriptingObject::GetScriptingClass() { return pParent; }
}
