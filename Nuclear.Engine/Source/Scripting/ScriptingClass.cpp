#include "ManagedRuntime.h"
#include <Scripting/ScriptingClass.h>
#include <Scripting/ScriptingModule.h>
#include <Nuclear/Managed/Type.hpp>
#include <Utilities/Logger.h>

namespace Nuclear
{
	namespace Scripting
	{
		ScriptingObject ScriptingClass::CreateObject(Uint32 entityID)
		{
			ScriptingObject result;
			if (mRuntimeLifetime.expired() || !ScriptingModule::Get().IsInitialized() || !pClass || !*pClass)
				return result;

			result.pObject = std::make_shared<Nuclear::Managed::ManagedObject>(pClass->CreateInstance());
			if (!result.pObject->IsValid())
			{
				NUCLEAR_ERROR("[ScriptingClass] Failed to construct {0}", mDesc.mFullName);
				return {};
			}
			result.pParent = this;
			ScriptingModule::Get().TrackObject(result.pObject);
			result.pObject->InvokeMethod("BindEntity", entityID);
			return result;
		}
		Nuclear::Managed::Type* ScriptingClass::GetClassPtr()
		{
			return mRuntimeLifetime.expired() ? nullptr : pClass;
		}
	}
}
