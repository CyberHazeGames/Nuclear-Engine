#pragma once
#include <NE_Common.h>
#include <memory>
#include <string>

namespace Nuclear::Managed { class ManagedObject; }

namespace Nuclear
{
	namespace Scripting
	{
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
			std::shared_ptr<Nuclear::Managed::ManagedObject> pObject;
			ScriptingClass* pParent = nullptr;
		};
	}
}
