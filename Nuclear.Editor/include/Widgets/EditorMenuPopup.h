#pragma once
#include <UI/PanelWidget.h>
#include <functional>
#include <string>

namespace Nuclear::Editor
{
	/// Full-viewport input barrier containing a compact dropdown. Context owns its lifetime.
	class EditorMenuPopup : public UI::PanelWidget
	{
	public:
		/// Entries use copied labels/callbacks; an empty callback displays a disabled command.
		using Items = std::vector<std::pair<std::string, std::function<void()>>>;
		/// Copies commands and callbacks. Anchor uses logical units; dismiss runs before a command.
		/// Navigate handles header hover/click; switchMenu receives -1/1 for Left/Right navigation.
		EditorMenuPopup(UI::Rect anchor, const Items& items, std::function<void()> dismiss,
			std::function<void(UI::Vec2, bool)> navigate, std::function<void(int)> switchMenu);
		/// Handles menu navigation, command activation and dismissal on outside click/Escape/focus loss.
		UI::EventReply OnInput(const UI::InputEvent& event) override;
		bool IsFocusable() const override { return true; } ///< Supports keyboard input on the dropdown backdrop.
	protected:
		bool AcceptsPointer() const override { return true; } ///< Blocks underlying editor actions until dismissal.
		void OnArrange(UI::Rect content) override; ///< Fits dropdown to the viewport and places command rows.
		void OnPaint(UI::DrawList& list, UI::Rect clip) const override; ///< Paints dropdown background and border.
	private:
		UI::Rect mAnchor; ///< Opening menu button bounds in logical units.
		UI::Rect mPopup; ///< Arranged dropdown geometry in logical units.
		Items mItems; ///< Commands retained for keyboard selection/activation.
		size_t mSelected = size_t(-1); ///< Keyboard-selected enabled command, or none.
		std::function<void()> mDismiss; ///< Safe context-deferred removal callback.
		std::function<void(UI::Vec2, bool)> mNavigate; ///< Routes header hover/click back to the title bar.
		std::function<void(int)> mSwitchMenu; ///< Opens the previous/next header on Left/Right.
	};
}
