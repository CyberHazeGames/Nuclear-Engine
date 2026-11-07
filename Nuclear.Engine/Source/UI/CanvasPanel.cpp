#include "WidgetUtilities.h"
#include <UI/CanvasPanel.h>

namespace Nuclear::UI
{
	void CanvasPanel::OnArrange(Rect content)
	{
		for (auto& entry : mChildren)
		{
			const auto& slot = entry.Layout;
			const float left = Detail::UnitValue(slot.AnchorMin.x);
			const float top = Detail::UnitValue(slot.AnchorMin.y);
			const float right = std::max(left, Detail::UnitValue(slot.AnchorMax.x));
			const float bottom = std::max(top, Detail::UnitValue(slot.AnchorMax.y));
			const auto desired = entry.Child->GetDesiredSize();
			float width = right > left ? content.Width * (right - left) - slot.Offsets.x - slot.Offsets.Width
									   : (slot.AutoSize ? desired.x : slot.Offsets.Width);
			float height = bottom > top ? content.Height * (bottom - top) - slot.Offsets.y - slot.Offsets.Height
										: (slot.AutoSize ? desired.y : slot.Offsets.Height);
			width = std::max(0.0f, width);
			height = std::max(0.0f, height);
			float x = content.x + content.Width * left + slot.Offsets.x - (right == left ? width * slot.Pivot.x : 0);
			float y = content.y + content.Height * top + slot.Offsets.y - (bottom == top ? height * slot.Pivot.y : 0);
			ArrangeChild(entry, {x, y, width, height});
		}
	}
} // namespace Nuclear::UI
