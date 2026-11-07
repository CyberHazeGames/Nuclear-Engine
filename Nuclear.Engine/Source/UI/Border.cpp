#include "WidgetUtilities.h"
#include <UI/Border.h>
#include <UI/DrawList.h>

namespace Nuclear::UI
{
	void Border::OnPaint(DrawList& list, Rect clip) const
	{
		list.Add(GetBounds(), clip, mBrush.Tint, Detail::BrushTexture(mBrush), mBrush.UV);
	}
} // namespace Nuclear::UI
