#include <Scripting/ScriptingClass.h>
#include <Scripting/IScriptingBackend.h>

namespace Nuclear::Scripting
{
	ScriptingObject ScriptingClass::CreateObject(Uint32 entityID) const
	{
		ScriptingObject result;
		if (pClass)
		{
			result.pObject = pClass->CreateObject(entityID);
			if (result.pObject)
				result.pParent = const_cast<ScriptingClass*>(this);
		}
		return result;
	}

	bool ScriptingClass::IsValid() const { return pClass != nullptr; }
}
