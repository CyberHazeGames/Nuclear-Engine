#pragma once
#include <UI/ScrollBox.h>
#include <functional>

namespace Nuclear::Editor
{
	class EditorHistory;
	/// Dockable settings for the active scene. The workspace owns this widget and its property controls.
	class SceneSettings : public UI::ScrollBox
	{
	public:
		/// Borrows history for the panel's lifetime and copies changed to request refresh outside dispatch.
		/// Objects captured by changed must remain alive until the panel is destroyed.
		SceneSettings(EditorHistory& history, std::function<void()> changed);
		void Refresh(); ///< Rebuilds controls after a scene change, committed settings edit or history replay.
	private:
		EditorHistory& mHistory; ///< Shared scene/entity transaction order; lives until editor shutdown.
		std::function<void()> mChanged; ///< Requests deferred controller refresh without rebuilding during commit.
	};
}
