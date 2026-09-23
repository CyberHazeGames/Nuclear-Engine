#pragma once
#include <NE_Common.h>
#include <string>

namespace Nuclear::Managed { class ManagedAssembly; }

namespace Nuclear
{
	namespace Scripting
	{
		class NEAPI ScriptingAssembly
		{
		public:
			const std::string& GetNamespaceName() const;
			Nuclear::Managed::ManagedAssembly* GetAssembly();
		private:
			friend class ScriptingModule;
			Nuclear::Managed::ManagedAssembly* pAssembly = nullptr;
			std::string mNamespaceName;
		};
	}
}
