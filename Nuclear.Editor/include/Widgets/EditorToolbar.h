#pragma once

#include <UI/PanelWidget.h>

namespace Nuclear::Editor
{
	/// Owns editor action widgets, wrapping them into rows without reducing their text size.
	/// Children keep their desired size and slot padding/alignment; fill weights are unused.
	class EditorToolbar : public UI::PanelWidget
	{
	protected:
		/// Measures children against the available logical width and returns the wrapped row extent.
		UI::Vec2 OnMeasure(UI::Vec2 available) override;
		/// Arranges children in insertion order with eight logical units between actions and rows.
		void OnArrange(UI::Rect content) override;
	};
}
