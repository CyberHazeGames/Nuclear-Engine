#include <Components\ScriptComponent.h>

namespace Nuclear
{
	namespace Components
	{
		ScriptComponent::ScriptComponent()
			: pParentScript(nullptr)
		{
		}
		ScriptComponent::ScriptComponent(Assets::Script* asset) : ScriptComponent(asset, {}) {}
		ScriptComponent::ScriptComponent(Assets::Script* asset, std::string fields)
			: Fields(std::move(fields)), pParentScript(asset)
		{
		}
		ScriptComponent::~ScriptComponent()
		{
		}
		Assets::Script* ScriptComponent::GetScriptAsset()
		{
			return pParentScript;
		}
		Scripting::ScriptingObject& ScriptComponent::GetScriptingObject()
		{
			return mScriptObject;
		}
	}
}