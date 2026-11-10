#pragma once
#include <Common.h>
#include <functional>
#include <MeshPreviewCache.h>
#include <unordered_map>
#include <unordered_set>
namespace Nuclear::Editor
{
	class EditorHistory;
	class EditorColorPickerWindow;
	/// Builds an inspector for a validated ECS handle. Callbacks resolve components on each edit.
	class EntityEditor
	{
	  public:
		/// Returns an owning inspector tree. The callback requests panel refresh;
		/// its argument is true when structural edits also require new renderer bindings.
		/// History outlives the returned tree and records committed property/component edits.
		/// Component disclosure is retained per versioned entity until Reset and does not enter scene history.
		/// colors owns external choosers and must outlive the returned property tree.
		std::unique_ptr<UI::Widget> Build(entt::entity entity, std::function<void(bool)> changed,
			EditorHistory& history, EditorColorPickerWindow& colors);
		/// Clears disclosure state and cached thumbnails when the scene/session is replaced.
		/// Rebuild/remove the old inspector before its next paint; does not edit ECS data.
		void Reset();
	private:
		/// Creates a component header and returns its owned property column. Empty remove disables X.
		/// Callbacks borrow this inspector controller, which must outlive the returned tree.
		UI::VerticalBox& ComponentSection(UI::PanelWidget& parent, entt::entity entity,
			entt::id_type component, const std::string& title, std::function<void()> remove);
		std::unordered_map<entt::entity, std::unordered_set<entt::id_type>> mCollapsedComponents;
		///< Hidden property bodies indexed by versioned entity identity and component type, until Reset.
		MeshPreviewCache mAssetPreviews; ///< Owns mesh/material thumbnails until scene reset or editor shutdown.
	};
} // namespace Nuclear::Editor
