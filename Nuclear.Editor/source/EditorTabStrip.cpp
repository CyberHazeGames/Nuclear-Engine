#include <EditorTabStrip.h>
#include <UI/DrawList.h>
#include <Nuclear.Platform/Input.h>
#include <algorithm>
#include <cmath>
#include <utility>

namespace Nuclear::Editor
{
	EditorTabStrip::EditorTabStrip()
	{
		SetSizeLimits({0, 34}, {1000000, 34});
	}

	void EditorTabStrip::SetTabs(const std::vector<Tab>& tabs, const std::string& active)
	{
		mRevealActive = mRevealActive || active != mActive;
		mTabs = tabs;
		mActive = active;
		InvalidateLayout();
	}

	void EditorTabStrip::OnArrange(UI::Rect content)
	{
		const float available = std::max(0.f, content.Width - 30);
		const float width = mTabs.empty() ? 140.f : std::clamp(available / mTabs.size(), 125.f, 230.f);
		const float maximum = std::max(0.f, width * mTabs.size() - available);
		mScroll = std::clamp(mScroll, 0.f, maximum);
		if (mRevealActive)
		{
			for (size_t index = 0; index < mTabs.size(); ++index)
			{
				if (mTabs[index].Id != mActive)
					continue;
				mScroll = std::clamp(mScroll, std::max(0.f, (index + 1) * width - available), index * width);
			}
			mRevealActive = false;
		}
		mBounds.clear();
		for (size_t index = 0; index < mTabs.size(); ++index)
			mBounds.push_back({content.x + index * width - mScroll, content.y, width - 3, content.Height});
	}

	UI::Rect EditorTabStrip::GetTabBounds(const std::string& id) const
	{
		auto clip = GetBounds();
		clip.Width = std::max(0.f, clip.Width - 30);
		for (size_t index = 0; index < mTabs.size() && index < mBounds.size(); ++index)
			if (mTabs[index].Id == id)
				return mBounds[index].Intersect(clip);
		return {};
	}

	size_t EditorTabStrip::InsertionAt(UI::Vec2 point) const
	{
		for (size_t index = 0; index < mBounds.size(); ++index)
			if (point.x < mBounds[index].x + mBounds[index].Width * .5f)
				return index;
		return mTabs.size();
	}

	UI::EventReply EditorTabStrip::OnInput(const UI::InputEvent& event)
	{
		using Type = UI::InputEvent::Type;
		if (event.EventType == Type::Cancel ||
			(event.EventType == Type::KeyDown && event.Key == Platform::Input::KEYCODE_ESCAPE))
		{
			if (mDragging && OnDragCancelled)
				OnDragCancelled();
			mPressed.clear();
			mDragging = false;
			return {true, false, true};
		}
		if (event.EventType == Type::Wheel)
		{
			mScroll -= event.WheelY * 100;
			InvalidateLayout();
			return {true};
		}
		if (event.EventType == Type::KeyDown && !mTabs.empty() &&
			(event.Key == Platform::Input::KEYCODE_LEFT || event.Key == Platform::Input::KEYCODE_RIGHT))
		{
			size_t index = 0;
			for (size_t i = 0; i < mTabs.size(); ++i)
				if (mTabs[i].Id == mActive)
					index = i;
			index = event.Key == Platform::Input::KEYCODE_RIGHT ? (index + 1) % mTabs.size()
				: (index + mTabs.size() - 1) % mTabs.size();
			if (OnSelected)
				OnSelected(mTabs[index].Id);
			return {true};
		}
		if (event.EventType == Type::PointerDown && event.Button == MOUSE_BUTTON_LEFT)
		{
			mAction = 0;
			mPressed.clear();
			mDragging = false;
			mStart = event.Position;
			const auto bounds = GetBounds();
			if (event.Position.x >= bounds.x + bounds.Width - 30)
			{
				for (const auto& tab : mTabs)
					if (tab.Id == mActive && tab.Detachable)
						mPressed = mActive;
				mAction = 2;
			}
			else
			{
				for (size_t index = 0; index < mTabs.size(); ++index)
				{
					if (!GetTabBounds(mTabs[index].Id).Contains(event.Position))
					continue;
					mPressed = mTabs[index].Id;
					if (mTabs[index].Closable && event.Position.x >= mBounds[index].x + mBounds[index].Width - 24)
						mAction = 1;
					break;
				}
			}
			return {true, !mPressed.empty(), false, true};
		}
		if (event.EventType == Type::PointerMove && !mPressed.empty() && mAction == 0)
		{
			if (std::hypot(event.Position.x - mStart.x, event.Position.y - mStart.y) > 6)
				mDragging = true;
			if (mDragging && OnDragged)
				OnDragged(mPressed, event.Position, false);
			return {true};
		}
		if (event.EventType == Type::PointerUp && event.Button == MOUSE_BUTTON_LEFT && !mPressed.empty())
		{
			const auto id = std::exchange(mPressed, {});
			if (mDragging)
			{
				mDragging = false;
				if (OnDragged)
					OnDragged(id, event.Position, true);
			}
			else if (GetBounds().Contains(event.Position))
			{
				const auto tab = GetTabBounds(id);
				if (mAction == 1 && OnClosed && tab.Contains(event.Position) && event.Position.x >= tab.x + tab.Width - 24)
					OnClosed(id);
				else if (mAction == 2 && OnDetached && event.Position.x >= GetBounds().x + GetBounds().Width - 30)
					OnDetached(id);
				else if (mAction == 0 && OnSelected && tab.Contains(event.Position))
					OnSelected(id);
			}
			return {true, false, true};
		}
		return {};
	}

	void EditorTabStrip::OnPaint(UI::DrawList& list, UI::Rect clip) const
	{
		const auto bounds = GetBounds();
		list.Add(bounds, clip, {.045f, .055f, .07f, 1});
		auto tabClip = clip.Intersect({bounds.x, bounds.y, std::max(0.f, bounds.Width - 30), bounds.Height});
		for (size_t index = 0; index < mTabs.size() && index < mBounds.size(); ++index)
		{
			const auto rect = mBounds[index];
			const bool active = mTabs[index].Id == mActive;
			list.Add(rect, tabClip, active ? UI::Color{.09f, .14f, .17f, 1} : UI::Color{.065f, .08f, .10f, 1});
			if (active)
				list.Add({rect.x, rect.y, rect.Width, 2}, tabClip, {.15f, .7f, .6f, 1});
			list.Text(ResolveFont(nullptr), mTabs[index].Title,
				{rect.x + 10, rect.y + 9, rect.Width - (mTabs[index].Closable ? 38.f : 18.f), 20},
				tabClip, 14, {.8f, .86f, .9f, 1}, UI::Alignment::Start, false);
			if (mTabs[index].Closable)
				list.Text(ResolveFont(nullptr), "x", {rect.x + rect.Width - 20, rect.y + 9, 18, 20},
					tabClip, 13, {.6f, .67f, .72f, 1}, UI::Alignment::Start, false);
		}
		for (const auto& tab : mTabs)
		{
			if (tab.Id != mActive || !tab.Detachable)
				continue;
			const UI::Rect icon = {bounds.x + bounds.Width - 23, bounds.y + 10, 14, 12};
			const UI::Color color = {.5f, .7f, .72f, 1};
			list.Add({icon.x, icon.y, icon.Width, 1}, clip, color);
			list.Add({icon.x, icon.y + icon.Height - 1, icon.Width, 1}, clip, color);
			list.Add({icon.x, icon.y, 1, icon.Height}, clip, color);
			list.Add({icon.x + icon.Width - 1, icon.y, 1, icon.Height}, clip, color);
			list.Add({icon.x + 3, icon.y + 3, icon.Width - 6, 1}, clip, color);
		}
		if (HasFocus())
			list.Add({bounds.x, bounds.y + bounds.Height - 1, bounds.Width, 1}, clip, {.15f, .7f, .6f, 1});
	}
}
