#pragma once
#include <UI/Button.h>

namespace Nuclear::Editor
{
	/// Asset tile button that distinguishes an ordinary click from a captured mesh drag.
	class EditorAssetButton : public UI::Button
	{
	public:
		/// Reports logical coordinates in this button's context; true indicates the final release.
		using DragCallback = std::function<void(UI::UIContext&, UI::Vec2, bool)>;
		EditorAssetButton(DragCallback dragged, std::function<void()> cancelled); ///< Copies window-thread callbacks.
		UI::EventReply OnInput(const UI::InputEvent& event) override; ///< Six-unit threshold; Escape/focus loss cancels without click activation.
	protected:
		void OnRemoved() override; ///< Cancels active placement before callbacks can outlive their source.
	private:
		void CancelDrag(); ///< Clears the gesture and notifies its owner once.
		DragCallback mDragged; ///< Reports captured movement/release using the source context.
		std::function<void()> mCancelled; ///< Removes any outstanding placement preview.
		UI::Vec2 mStart; ///< Initial logical press position.
		bool mPressed = false; ///< A left press may become a click or a drag.
		bool mDragging = false; ///< Movement crossed the drag threshold.
	};
}
