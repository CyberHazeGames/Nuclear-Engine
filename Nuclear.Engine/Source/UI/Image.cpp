#include "WidgetUtilities.h"
#include <UI/DrawList.h>
#include <UI/Image.h>

namespace Nuclear::UI
{
	void Image::OnPaint(DrawList& list, Rect clip) const
	{
		list.Add(ContentBounds(), clip, mBrush.Tint, Detail::BrushTexture(mBrush), mBrush.UV);
	}
} // namespace Nuclear::UI
