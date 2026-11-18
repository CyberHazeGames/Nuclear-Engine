#pragma once
#include <Assets/Script.h>
#include <Scripting/ScriptingObject.h>

namespace Nuclear
{
	namespace Components
	{

		class NEAPI ScriptComponent
		{
		public:
			ScriptComponent();
			ScriptComponent(Assets::Script* asset);
			ScriptComponent(Assets::Script* asset, std::string fields); ///< Initializes a class with authored JSON field overrides before OnStart.
			~ScriptComponent();

			Assets::Script* GetScriptAsset(); 
			Scripting::ScriptingObject& GetScriptingObject();
			/// Fully qualified managed class assigned by an author. Resolved in the game process at startup.
			std::string ClassName;
			std::string Fields; ///< JSON object of per-instance public C# field values.
		protected:
			Assets::Script* pParentScript;
			Scripting::ScriptingObject mScriptObject;
		};
	}
}