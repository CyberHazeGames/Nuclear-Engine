#include "WidgetUtilities.h"
#include <UI/DrawList.h>
#include <UI/ProgressBar.h>

namespace Nuclear::UI
{
	void ProgressBar::SetValue(float value)
	{
		mValue = Detail::UnitValue(value);
	}

	void ProgressBar::SetColors(Color background, Color fill)
	{
		mBackground = background;
		mFill = fill;
	}

	void ProgressBar::OnPaint(DrawList& list, Rect clip) const
	{
		auto bounds = ContentBounds();
		list.Add(bounds, clip, mBackground);
		bounds.Width *= mValue;
		list.Add(bounds, clip, mFill);
	}
} // namespace Nuclear::UI
