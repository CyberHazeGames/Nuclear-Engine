#include <EditorToolbar.h>

namespace Nuclear::Editor
{
	UI::Vec2 EditorToolbar::OnMeasure(UI::Vec2 available)
	{
		UI::Vec2 extent;
		float rowWidth = 0;
		float rowHeight = 0;

		for (auto& entry : mChildren)
		{
			const auto& padding = entry.Layout.Padding;
			entry.Child->Measure({std::max(0.f, available.x - padding.Left - padding.Right), available.y});
			if (entry.Child->GetVisibility() == UI::Visibility::Collapsed)
				continue;

			const auto desired = entry.Child->GetDesiredSize();
			const float width = desired.x + padding.Left + padding.Right;
			const float height = desired.y + padding.Top + padding.Bottom;
			if (rowWidth > 0 && rowWidth + 8 + width > available.x)
			{
				extent.x = std::max(extent.x, rowWidth);
				extent.y += rowHeight + 8;
				rowWidth = rowHeight = 0;
			}

			rowWidth += (rowWidth > 0 ? 8 : 0) + width;
			rowHeight = std::max(rowHeight, height);
		}

		return {std::max(extent.x, rowWidth), extent.y + rowHeight};
	}

	void EditorToolbar::OnArrange(UI::Rect content)
	{
		float x = 0;
		float y = 0;
		float rowHeight = 0;

		for (auto& entry : mChildren)
		{
			if (entry.Child->GetVisibility() == UI::Visibility::Collapsed)
			{
				entry.Child->Arrange({content.x, content.y, 0, 0});
				continue;
			}

			const auto desired = entry.Child->GetDesiredSize();
			const auto& padding = entry.Layout.Padding;
			const float width = desired.x + padding.Left + padding.Right;
			const float height = desired.y + padding.Top + padding.Bottom;
			if (x > 0 && x + width > content.Width)
			{
				x = 0;
				y += rowHeight + 8;
				rowHeight = 0;
			}

			ArrangeChild(entry, {content.x + x, content.y + y, width, height});
			x += width + 8;
			rowHeight = std::max(rowHeight, height);
		}
	}
}
