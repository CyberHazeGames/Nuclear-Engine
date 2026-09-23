#include <Scripting/ScriptingAssembly.h>

namespace Nuclear
{
	namespace Scripting
	{
		const std::string& ScriptingAssembly::GetNamespaceName() const
		{
			return mNamespaceName;
		}
		Nuclear::Managed::ManagedAssembly* ScriptingAssembly::GetAssembly()
		{
			return pAssembly;
		}
	}
}
