#pragma once
#include <Assets/IAsset.h>
#include <Scripting/ScriptingClass.h>

namespace Nuclear
{
	namespace Assets
	{

		class NEAPI Script : public IAsset
		{
		public:
			Script();
			~Script();


			std::string mOnStartMethod;
			std::string mOnUpdateMethod;

			Scripting::ScriptingClass mClass;

		};
	}
}
