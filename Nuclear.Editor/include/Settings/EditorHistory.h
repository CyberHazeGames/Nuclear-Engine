#pragma once
#include <EditorEntityState.h>
#include <EditorSceneState.h>
#include <optional>
#include <functional>

namespace Nuclear::Editor
{
	/// Bounded scene-authoring history, used on the editor thread. Clear before replacing the scene.
	class EditorHistory
	{
	public:
		/// Runs one validated edit and records it only when authored values change.
		void Edit(entt::entity entity, const std::string& label, const std::function<void()>& edit);
		/// Runs one scene name/clear-color edit, interleaved with entity actions; unchanged values add nothing.
		void EditScene(const std::string& label, const std::function<void()>& edit);
		/// Records an already-created entity, including mesh instantiation from the asset browser.
		void RecordCreated(entt::entity entity);
		/// Records an already-completed drag as one action; canceled/no-op drags add nothing.
		void RecordTransform(entt::entity entity, const ECS::Transform& before);
		bool Undo(); ///< Restores the previous state; false when empty or the identity cannot be restored.
		bool Redo(); ///< Reapplies the next state; false when empty or the identity cannot be restored.
		bool CanUndo() const; ///< Whether an applied action exists.
		bool CanRedo() const; ///< Whether an undone action exists.
		void Clear(); ///< Releases snapshots and redo history without touching the registry.
		/// Called after successful replay with the restored selection (null after creation is undone).
		std::function<void(entt::entity)> OnRestored;
		std::function<void()> OnSceneRestored; ///< Requests settings/title refresh after scene replay; preserves entity selection.
	private:
		/// One entity or scene-settings transaction; each entry uses exactly one snapshot kind.
		struct Entry
		{
			entt::entity Entity; ///< Target identity; resolved anew for every replay.
			std::string Label; ///< Human-readable action name for diagnostics and future history UI.
			EditorEntityState Before; ///< Authored data immediately before the edit.
			EditorEntityState After; ///< Authored data immediately after the edit.
			std::optional<EditorSceneState> SceneBefore; ///< Present only for scene-settings transactions.
			std::optional<EditorSceneState> SceneAfter; ///< Matching scene-settings state after the edit.
		};
		void Record(Entry entry); ///< Drops redo only for actual changes; retains at most 128 actions.
		std::vector<Entry> mEntries; ///< Snapshots borrow session assets and own their value data.
		size_t mPosition = 0; ///< Number of currently applied entries.
	};
}
