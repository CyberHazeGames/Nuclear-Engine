#pragma once
#include <UI/ContentWidget.h>

namespace Nuclear::UI
{
	/// Clipped vertical single-child viewport. Wheel and PageUp/PageDown scroll in logical units.
	class NEAPI ScrollBox : public ContentWidget
	{
	  public:
		void SetOffset(float offset); ///< Clamps finite logical offset to the current content extent.
		float GetOffset() const
		{
			return mOffset;
		} ///< Current logical distance from the top.
		void ScrollIntoView(const Widget& widget); ///< Reveals an arranged descendant during focus traversal.
		bool IsFocusable() const override
		{
			return true;
		} ///< Allows keyboard paging.
		EventReply OnInput(const InputEvent& event) override; ///< Wheel bubbles at content boundaries.
	  protected:
		Vec2 OnMeasure(Vec2 available) override; ///< Measures unbounded content height and bounded viewport.
		void OnArrange(
			Rect content) override; ///< Clamps offset and arranges the content with a vertical translation.
		void OnPaint(DrawList& list,
					 Rect clip) const override; ///< Draws a proportional scroll position indicator.
	  private:
		float mOffset = 0; ///< Logical vertical scroll offset.
		float mExtent = 0; ///< Scrollable distance after arrangement.
	};
} // namespace Nuclear::UI
