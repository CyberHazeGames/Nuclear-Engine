#pragma once

#include <UI/Button.h>

namespace Nuclear::Editor
{
	/// Compact inspector header control with a font-independent disclosure arrow or removal cross.
	class EditorComponentButton : public UI::Button
	{
	public:
		/// The component-header action represented by the control.
		enum class Action { Collapse, Remove };
		/// Creates a 24-logical-unit button. The owning inspector supplies its activation callback.
		explicit EditorComponentButton(Action action);
		/// Changes the disclosure direction; true points down and false points right.
		void SetExpanded(bool expanded);
		/// Returns the semantic action so callers can distinguish header controls.
		Action GetAction() const;
	protected:
		/// Paints normal button state and its glyph, dimming disabled removal controls.
		void OnPaint(UI::DrawList& list, UI::Rect clip) const override;
	private:
		Action mAction; ///< Fixed header action; callbacks remain owned by the base button.
		bool mExpanded = true; ///< Whether the associated component's property body is visible.
	};
}
