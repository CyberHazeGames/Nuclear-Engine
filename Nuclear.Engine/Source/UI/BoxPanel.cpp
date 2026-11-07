#include <UI/BoxPanel.h>
#include <cmath>

namespace Nuclear::UI
{
	BoxPanel::BoxPanel(bool horizontal) : mHorizontal(horizontal) {}

	void BoxPanel::SetSpacing(float spacing)
	{
		mSpacing = std::isfinite(spacing) ? std::max(0.0f, spacing) : 0;
		InvalidateLayout();
	}

	Vec2 BoxPanel::OnMeasure(Vec2 available)
	{
		Vec2 size;
		unsigned count = 0;
		for (auto& entry : mChildren)
		{
			const auto& padding = entry.Layout.Padding;
			entry.Child->Measure({std::max(0.0f, available.x - padding.Left - padding.Right),
								  std::max(0.0f, available.y - padding.Top - padding.Bottom)});
			if (entry.Child->GetVisibility() == Visibility::Collapsed)
				continue;
			const auto desired = entry.Child->GetDesiredSize();
			const float width = desired.x + padding.Left + padding.Right;
			const float height = desired.y + padding.Top + padding.Bottom;
			if (mHorizontal)
			{
				size.x += width;
				size.y = std::max(size.y, height);
			}
			else
			{
				size.y += height;
				size.x = std::max(size.x, width);
			}
			++count;
		}
		if (count > 1)
		{
			if (mHorizontal)
				size.x += (count - 1) * mSpacing;
			else
				size.y += (count - 1) * mSpacing;
		}
		return size;
	}

	void BoxPanel::OnArrange(Rect content)
	{
		float fixed = 0;
		float totalFill = 0;
		unsigned count = 0;
		for (const auto& entry : mChildren)
		{
			if (entry.Child->GetVisibility() == Visibility::Collapsed)
				continue;
			++count;
			const auto desired = entry.Child->GetDesiredSize();
			const auto& padding = entry.Layout.Padding;
			const float insets = mHorizontal ? padding.Left + padding.Right : padding.Top + padding.Bottom;
			fixed += insets;
			if (entry.Layout.Fill > 0)
				totalFill += entry.Layout.Fill;
			else
				fixed += mHorizontal ? desired.x : desired.y;
		}
		if (count > 1)
			fixed += (count - 1) * mSpacing;
		const float remaining = std::max(0.0f, (mHorizontal ? content.Width : content.Height) - fixed);
		float offset = 0;
		for (auto& entry : mChildren)
		{
			if (entry.Child->GetVisibility() == Visibility::Collapsed)
			{
				entry.Child->Arrange({content.x, content.y, 0, 0});
				continue;
			}
			const auto desired = entry.Child->GetDesiredSize();
			const auto& padding = entry.Layout.Padding;
			const float insets = mHorizontal ? padding.Left + padding.Right : padding.Top + padding.Bottom;
			const float extent = (entry.Layout.Fill > 0 && totalFill > 0 ? remaining * entry.Layout.Fill / totalFill
																		 : (mHorizontal ? desired.x : desired.y)) +
								 insets;
			Rect allocation = content;
			if (mHorizontal)
			{
				allocation.x += offset;
				allocation.Width = extent;
			}
			else
			{
				allocation.y += offset;
				allocation.Height = extent;
			}
			ArrangeChild(entry, allocation);
			offset += extent + mSpacing;
		}
	}
} // namespace Nuclear::UI
