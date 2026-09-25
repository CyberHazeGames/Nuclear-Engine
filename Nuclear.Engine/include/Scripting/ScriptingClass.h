#pragma once
#include <NE_Common.h>
#include <Scripting/ScriptingObject.h>
#include <memory>

namespace Nuclear::Scripting
{
	class IScriptClass;
	class ScriptingModule;

	class NEAPI ScriptingClass
	{
	public:
		ScriptingObject CreateObject(Uint32 entityID = 0) const;
		bool IsValid() const;
	private:
		friend class ScriptingModule;
		std::shared_ptr<IScriptClass> pClass;
	};
}
