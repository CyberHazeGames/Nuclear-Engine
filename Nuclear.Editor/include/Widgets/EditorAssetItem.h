#pragma once
#include <UI/Border.h>
#include <EditorMenuPopup.h>

namespace Nuclear::Editor
{
	/// Wraps one asset card, preserving left-click behavior and owning its context-menu lifetime.
	class EditorAssetItem : public UI::Border
	{
	public:
		/// Commands are copied and invoked on the window thread; children remain owned by this card.
		explicit EditorAssetItem(EditorMenuPopup::Items commands);
		/// Opens the menu at a right-button press in logical viewport coordinates.
		UI::EventReply OnInput(const UI::InputEvent& event) override;
	protected:
		bool AcceptsPointer() const override { return true; } ///< Includes gaps within the card.
		void OnRemoved() override; ///< Closes the overlay before its card leaves the context.
	private:
		void CloseMenu(); ///< Removes the context-owned popup safely, including during input dispatch.
		EditorMenuPopup::Items mCommands; ///< Stable-identity actions supplied by the browser.
		UI::WidgetId mPopup = 0; ///< Context-owned overlay, or zero while closed.
	};
}
