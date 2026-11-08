#include <UI/ScrollBox.h>
#include <UI/DrawList.h>
#include <Nuclear.Platform/Input.h>
#include <cmath>

namespace Nuclear::UI
{
	void ScrollBox::SetOffset(float offset)
	{
		mOffset = std::clamp(std::isfinite(offset) ? offset : 0, 0.0f, mExtent);
		InvalidateLayout();
	}

	Vec2 ScrollBox::OnMeasure(Vec2 available)
	{
		if (mChildren.empty())
			return {};
		mChildren.front().Child->Measure({std::max(0.0f, available.x - 8), 1000000});
		auto size = mChildren.front().Child->GetDesiredSize();
		return {std::min(available.x, size.x + 8), std::min(available.y, size.y)};
	}

	void ScrollBox::OnArrange(Rect content)
	{
		if (mChildren.empty())
		{
			mExtent = mOffset = 0;
			return;
		}
		auto& child = *mChildren.front().Child;
		float height = std::max(content.Height, child.GetDesiredSize().y);
		mExtent = height - content.Height;
		mOffset = std::clamp(mOffset, 0.0f, mExtent);
		child.Arrange({content.x, content.y - mOffset, std::max(0.0f, content.Width - 8), height});
	}

	void ScrollBox::ScrollIntoView(const Widget& widget)
	{
		auto viewport = ContentBounds();
		auto bounds = widget.GetBounds();
		if (bounds.y < viewport.y)
			SetOffset(mOffset + bounds.y - viewport.y);
		else if (bounds.y + bounds.Height > viewport.y + viewport.Height)
			SetOffset(mOffset + bounds.y + bounds.Height - viewport.y - viewport.Height);
	}

	EventReply ScrollBox::OnInput(const InputEvent& event)
	{
		float offset = mOffset;
		if (event.EventType == InputEvent::Type::Wheel)
			offset -= event.WheelY * 48;
		else if (event.EventType == InputEvent::Type::KeyDown)
		{
			if (event.Key == Platform::Input::KEYCODE_PAGEDOWN)
				offset += ContentBounds().Height * .8f;
			else if (event.Key == Platform::Input::KEYCODE_PAGEUP)
				offset -= ContentBounds().Height * .8f;
			else
				return {};
		}
		else if (event.EventType == InputEvent::Type::PointerDown)
			return {true, false, false, true};
		else
			return {};
		float previous = mOffset;
		SetOffset(offset);
		return {previous != mOffset};
	}

	void ScrollBox::OnPaint(DrawList& list, Rect clip) const
	{
		if (mExtent <= 0)
			return;
		auto bounds = ContentBounds();
		float height = std::max(12.0f, bounds.Height * bounds.Height / (bounds.Height + mExtent));
		list.Add(
			{bounds.x + bounds.Width - 4, bounds.y + (bounds.Height - height) * mOffset / mExtent, 3, height},
			clip, {.15f, .4f, .45f, 1});
	}
} // namespace Nuclear::UI
