#include <UI/PanelWidget.h>
#include <UI/UIContext.h>
#include <stdexcept>

namespace Nuclear::UI
{
	namespace
	{
		float AlignedOffset(float space, float size, Alignment alignment)
		{
			if (alignment == Alignment::Center)
				return (space - size) * .5f;
			if (alignment == Alignment::End)
				return space - size;
			return 0;
		}
	} // namespace

	Widget& PanelWidget::AddChild(std::unique_ptr<Widget> child, const Slot& slot)
	{
		if (!child || child->GetParent() || child->GetContext())
			throw std::invalid_argument("UI child must be nonnull and detached");
		auto& result = *child;
		mChildren.push_back({std::move(child), slot});
		result.Attach(GetContext(), this);
		InvalidateLayout();
		return result;
	}

	void PanelWidget::RemoveChild(WidgetId id)
	{
		if (auto* context = GetContext())
		{
			const auto parent = GetId();
			context->Defer(
				[context, parent, id]()
				{
					auto* panel = dynamic_cast<PanelWidget*>(context->Find(parent));
					if (!panel)
						return;
					for (auto iterator = panel->mChildren.begin(); iterator != panel->mChildren.end(); ++iterator)
					{
						if (iterator->Child->GetId() != id)
							continue;
						context->CancelSubtree(*iterator->Child);
						auto removed = std::move(iterator->Child);
						panel->mChildren.erase(iterator);
						removed->Attach(nullptr, nullptr);
						panel->InvalidateLayout();
						break;
					}
				});
			return;
		}
		std::erase_if(mChildren, [id](const ChildEntry& entry) { return entry.Child->GetId() == id; });
	}

	bool PanelWidget::SetSlot(WidgetId id, const Slot& slot)
	{
		for (auto& entry : mChildren)
		{
			if (entry.Child->GetId() == id)
			{
				entry.Layout = slot;
				InvalidateLayout();
				return true;
			}
		}
		return false;
	}

	Slot PanelWidget::GetSlot(WidgetId id) const
	{
		for (const auto& entry : mChildren)
			if (entry.Child->GetId() == id)
				return entry.Layout;
		return {};
	}

	std::vector<Widget*> PanelWidget::Children() const
	{
		std::vector<const ChildEntry*> ordered;
		for (const auto& child : mChildren)
			ordered.push_back(&child);
		std::stable_sort(ordered.begin(), ordered.end(),
						 [](auto* left, auto* right) { return left->Layout.ZOrder < right->Layout.ZOrder; });
		std::vector<Widget*> result;
		for (auto* entry : ordered)
			result.push_back(entry->Child.get());
		return result;
	}

	Vec2 PanelWidget::OnMeasure(Vec2 available)
	{
		Vec2 size;
		for (auto& entry : mChildren)
		{
			const auto& padding = entry.Layout.Padding;
			entry.Child->Measure({std::max(0.0f, available.x - padding.Left - padding.Right),
								  std::max(0.0f, available.y - padding.Top - padding.Bottom)});
			if (entry.Child->GetVisibility() == Visibility::Collapsed)
				continue;
			auto desired = entry.Child->GetDesiredSize();
			size.x = std::max(size.x, desired.x + padding.Left + padding.Right);
			size.y = std::max(size.y, desired.y + padding.Top + padding.Bottom);
		}
		return size;
	}

	void PanelWidget::ArrangeChild(ChildEntry& entry, Rect allocation)
	{
		if (entry.Child->GetVisibility() == Visibility::Collapsed)
		{
			entry.Child->Arrange({allocation.x, allocation.y, 0, 0});
			return;
		}
		const auto& padding = entry.Layout.Padding;
		allocation.x += padding.Left;
		allocation.y += padding.Top;
		allocation.Width = std::max(0.0f, allocation.Width - padding.Left - padding.Right);
		allocation.Height = std::max(0.0f, allocation.Height - padding.Top - padding.Bottom);
		// Remeasure at the allocated width so wrapping agrees with painting.
		entry.Child->Measure({allocation.Width, allocation.Height});
		const auto desired = entry.Child->GetDesiredSize();
		float width =
			entry.Layout.Horizontal == Alignment::Fill ? allocation.Width : std::min(allocation.Width, desired.x);
		float height =
			entry.Layout.Vertical == Alignment::Fill ? allocation.Height : std::min(allocation.Height, desired.y);
		entry.Child->Arrange({allocation.x + AlignedOffset(allocation.Width, width, entry.Layout.Horizontal),
							  allocation.y + AlignedOffset(allocation.Height, height, entry.Layout.Vertical), width,
							  height});
	}

	void PanelWidget::OnArrange(Rect content)
	{
		for (auto& entry : mChildren)
			ArrangeChild(entry, content);
	}
} // namespace Nuclear::UI
