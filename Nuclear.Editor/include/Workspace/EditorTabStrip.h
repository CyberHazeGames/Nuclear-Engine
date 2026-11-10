#pragma once
#include <UI/Widget.h>
#include <functional>
#include <string>
#include <vector>

namespace Nuclear::Editor
{
	/// Retained, scrollable tab header with selection, close, reorder and native-window tear-off gestures.
	/// Callbacks request controller work; the strip never transfers or destroys tab contents.
	class EditorTabStrip : public UI::Widget
	{
	public:
		/// Display metadata; Id is stable across moves and is independent of the visible title.
		struct Tab
		{
			std::string Id; ///< Stable workspace identity.
			std::string Title; ///< UTF-8 caption.
			bool Closable = true; ///< Whether the tab displays its close action.
			bool Detachable = true; ///< Whether the active tab offers the native-window action.
		};
		EditorTabStrip(); ///< Creates a 34-logical-unit header.
		/// Copies the ordered display metadata and selected identity; retains horizontal scroll position.
		void SetTabs(const std::vector<Tab>& tabs, const std::string& active);
		/// Returns a tab's visible logical bounds after layout, or an empty rectangle if absent.
		UI::Rect GetTabBounds(const std::string& id) const;
		/// Returns the width-fitted UTF-8 caption after layout; long names retain both ends around "...".
		/// The full title and stable identity remain unchanged. Returns empty if the tab is absent.
		std::string GetDisplayTitle(const std::string& id) const;
		/// Returns a tab insertion position for a header drop, clamped to [0, tab count].
		size_t InsertionAt(UI::Vec2 point) const;
		std::function<void(const std::string&)> OnSelected; ///< Requests activation of a stable tab ID.
		std::function<void(const std::string&)> OnClosed; ///< Requests closing a closable tab.
		std::function<void(const std::string&)> OnDetached; ///< Requests popping the active tab into a window.
		std::function<void(const std::string&, UI::Vec2, bool)> OnDragged; ///< Pointer sample; true on drop.
		std::function<void()> OnDragCancelled; ///< Clears any pending docking preview.
		bool IsFocusable() const override { return true; } ///< Arrow keys cycle visible tabs.
		UI::EventReply OnInput(const UI::InputEvent& event) override; ///< Handles header gestures only.
	protected:
		void OnArrange(UI::Rect content) override; ///< Keeps active tabs in the scrollable header range.
		void OnPaint(UI::DrawList& list, UI::Rect clip) const override; ///< Paints labels and close/tear-off controls.
	private:
		/// Fits a caption to logical width using the current font, without splitting UTF-8 codepoints.
		std::string FitTitle(const std::string& title, float width) const;
		std::vector<Tab> mTabs; ///< Copied header order, no content ownership.
		std::vector<std::string> mDisplayTitles; ///< Measured captions, recomputed when tab geometry changes.
		std::vector<UI::Rect> mBounds; ///< Unclipped tab allocations in logical coordinates.
		std::string mActive; ///< Selected identity.
		std::string mPressed; ///< Identity captured by the current pointer gesture.
		UI::Vec2 mStart; ///< Pointer position at press.
		float mScroll = 0; ///< Horizontal header offset in logical units.
		bool mDragging = false; ///< Press exceeded the tear-off/reorder threshold.
		bool mRevealActive = true; ///< Next layout scrolls the selected tab into view.
		int mAction = 0; ///< Pending selection (0), close (1), or tear-off (2).
	};
}
