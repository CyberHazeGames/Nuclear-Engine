#include <EditorHistory.h>

namespace Nuclear::Editor
{
	void EditorHistory::Edit(entt::entity entity, const std::string& label, const std::function<void()>& edit)
	{
		auto before = EditorEntityState::Capture(entity);
		if (!before.Exists)
			return;
		edit();
		Record({entity, label, std::move(before), EditorEntityState::Capture(entity)});
	}

	void EditorHistory::RecordCreated(entt::entity entity)
	{
		Record({entity, "Create entity", {}, EditorEntityState::Capture(entity)});
	}

	void EditorHistory::EditScene(const std::string& label, const std::function<void()>& edit)
	{
		auto before = EditorSceneState::Capture();
		if (!before.IsValid())
			return;
		edit();
		auto after = EditorSceneState::Capture();
		if (!after.IsValid() || before == after)
			return;
		Record({entt::null, label, {}, {}, std::move(before), std::move(after)});
	}

	void EditorHistory::RecordTransform(entt::entity entity, const ECS::Transform& beforeTransform)
	{
		auto after = EditorEntityState::Capture(entity);
		auto before = after;
		before.Transform = beforeTransform;
		Record({entity, "Transform", std::move(before), std::move(after)});
	}

	void EditorHistory::Record(Entry entry)
	{
		if (!entry.SceneBefore && entry.Before == entry.After)
			return;
		mEntries.erase(mEntries.begin() + mPosition, mEntries.end());
		mEntries.push_back(std::move(entry));
		if (mEntries.size() > 128)
			mEntries.erase(mEntries.begin());
		mPosition = mEntries.size();
	}

	bool EditorHistory::CanUndo() const { return mPosition != 0; }
	bool EditorHistory::CanRedo() const { return mPosition < mEntries.size(); }
	void EditorHistory::Clear()
	{
		mEntries.clear();
		mPosition = 0;
	}

	bool EditorHistory::Undo()
	{
		if (!CanUndo())
			return false;
		const auto& entry = mEntries[mPosition - 1];
		if (!(entry.SceneBefore ? entry.SceneBefore->Restore() : entry.Before.Restore(entry.Entity)))
			return false;
		--mPosition;
		if (entry.SceneBefore && OnSceneRestored)
			OnSceneRestored();
		else if (!entry.SceneBefore && OnRestored)
			OnRestored(entry.Before.Exists ? entry.Entity : entt::null);
		return true;
	}

	bool EditorHistory::Redo()
	{
		if (!CanRedo())
			return false;
		const auto& entry = mEntries[mPosition];
		if (!(entry.SceneAfter ? entry.SceneAfter->Restore() : entry.After.Restore(entry.Entity)))
			return false;
		++mPosition;
		if (entry.SceneAfter && OnSceneRestored)
			OnSceneRestored();
		else if (!entry.SceneAfter && OnRestored)
			OnRestored(entry.After.Exists ? entry.Entity : entt::null);
		return true;
	}
}
