#pragma once
#include <UI/Button.h>
#include <Nuclear.Platform/Window.h>

namespace Nuclear::Editor
{
	/// Caption control with a geometric glyph, independent of font symbol coverage.
	class EditorWindowButton : public UI::Button
	{
	public:
		/// Native action represented by this control.
		enum class Action { Minimize, Maximize, Close };
		/// Borrows a native window that must outlive this widget.
		EditorWindowButton(Platform::Window& window, Action action);
		Action GetAction() const { return mAction; } ///< Semantic action for inspection/accessibility.
	protected:
		void OnPaint(UI::DrawList& list, UI::Rect clip) const override; ///< Paints state and current restore/maximize glyph.
	private:
		Platform::Window& mWindow; ///< Borrowed window; never destroyed by this control.
		Action mAction; ///< Native command invoked on release.
	};
}
