#include "ManagedRuntime.h"
#include <Scripting/ScriptingObject.h>
#include <Nuclear/Managed/ManagedObject.hpp>

namespace Nuclear
{
	namespace Scripting
	{
		bool ScriptingObject::IsValid() const
		{
			return pObject && pObject->IsValid();
		}
		void ScriptingObject::CallMethod(const std::string& method)
		{
			if (IsValid() && !method.empty())
				pObject->InvokeMethod(method);
		}
		void ScriptingObject::CallMethod(const std::string& method, float value)
		{
			if (IsValid() && !method.empty())
				pObject->InvokeMethod(method, value);
		}
		ScriptingClass* ScriptingObject::GetScriptingClass()
		{
			return pParent;
		}
	}
}
