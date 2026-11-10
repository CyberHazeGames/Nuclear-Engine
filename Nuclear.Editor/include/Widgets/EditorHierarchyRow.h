#pragma once
#include <UI/Button.h>

namespace Nuclear::Editor
{
	/// Entity row with normal left-click selection and a right-click context-menu request.
	class EditorHierarchyRow : public UI::Button
	{
	public:
		EditorHierarchyRow(); ///< Uses the standard retained editor action palette and padding.
		/// Reports logical pointer coordinates in the owning context; the controller owns the popup.
		std::function<void(UI::UIContext&, UI::Vec2)> OnContextRequested;
		UI::EventReply OnInput(const UI::InputEvent& event) override; ///< Right-click requests commands; other input retains button behavior.
	};
}
