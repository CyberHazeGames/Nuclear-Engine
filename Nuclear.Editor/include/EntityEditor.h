#pragma once
#include <Common.h>
#include <functional>
namespace Nuclear::Editor
{
	class EditorHistory;
	/// Builds an inspector for a validated ECS handle. Callbacks resolve components on each edit.
	class EntityEditor
	{
	  public:
		/// Returns an owning inspector tree. The callback requests panel refresh;
		/// its argument is true when structural edits also require new renderer bindings.
		/// History outlives the returned tree and records committed property/component edits.
		std::unique_ptr<UI::Widget> Build(entt::entity entity, std::function<void(bool)> changed, EditorHistory& history);
	};
} // namespace Nuclear::Editor
