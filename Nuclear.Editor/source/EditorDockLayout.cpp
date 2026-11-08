#include <EditorDockLayout.h>
#include <UI/DrawList.h>
#include <UI/Border.h>
#include <UI/TextBlock.h>
#include <Nuclear.Platform/Input.h>
#include <cmath>

namespace Nuclear::Editor
{
	EditorDockLayout::EditorDockLayout()
	{
		for (auto& area : mAreas)
			area = &Add<EditorDockArea>();
		GetArea(EditorDockRegion::Documents).ShowHeader(false);
		pPreview = &Add<UI::Border>();
		pPreview->SetBrush({nullptr, {.08f, .42f, .44f, .45f}});
		pPreview->SetPadding({14, 12, 14, 12});
		pPreview->Add<UI::TextBlock>("Dock here");
		pPreview->SetVisibility(UI::Visibility::Collapsed);
	}

	EditorDockArea& EditorDockLayout::GetArea(EditorDockRegion region)
	{
		return *mAreas[static_cast<size_t>(region)];
	}

	void EditorDockLayout::ResetSplits()
	{
		mLeft = .21f;
		mRight = .27f;
		mBottom = .34f;
		mBottomSplit = .59f;
		InvalidateLayout();
	}

	void EditorDockLayout::SetDropPreview(int region)
	{
		mPreview = region;
		pPreview->SetVisibility(region >= 0 && region < 5 ? UI::Visibility::Visible : UI::Visibility::Collapsed);
		InvalidateLayout();
	}

	UI::Rect EditorDockLayout::RegionBounds(EditorDockRegion region, bool includeEmpty) const
	{
		const auto bounds = GetBounds();
		auto occupied = [this, includeEmpty](int index) { return includeEmpty || !mAreas[index]->GetTabs().empty(); };
		const float left = occupied(0) ? bounds.Width * mLeft : 0;
		const float right = occupied(2) ? bounds.Width * mRight : 0;
		const float bottom = occupied(3) || occupied(4) ? bounds.Height * mBottom : 0;
		const float inner = bounds.Width - right;
		const float split = !occupied(3) ? 0 : !occupied(4) ? inner : inner * mBottomSplit;
		switch (region)
		{
		case EditorDockRegion::Left: return {bounds.x, bounds.y, left, bounds.Height - bottom};
		case EditorDockRegion::Documents: return {bounds.x + left, bounds.y, inner - left, bounds.Height - bottom};
		case EditorDockRegion::Right: return {bounds.x + inner, bounds.y, right, bounds.Height};
		case EditorDockRegion::BottomLeft: return {bounds.x, bounds.y + bounds.Height - bottom, split, bottom};
		case EditorDockRegion::BottomRight: return {bounds.x + split, bounds.y + bounds.Height - bottom, inner - split, bottom};
		}
		return {};
	}

	EditorDockRegion EditorDockLayout::DropRegion(UI::Vec2 point) const
	{
		// Default-sized edge targets remain reachable when their groups are empty.
		for (auto region : {EditorDockRegion::Right, EditorDockRegion::BottomLeft,
			EditorDockRegion::BottomRight, EditorDockRegion::Left})
			if (RegionBounds(region, true).Contains(point))
				return region;
		return EditorDockRegion::Documents;
	}

	UI::Vec2 EditorDockLayout::OnMeasure(UI::Vec2 available)
	{
		PanelWidget::OnMeasure(available);
		return {1200, 650};
	}

	void EditorDockLayout::OnArrange(UI::Rect)
	{
		for (size_t index = 0; index < mAreas.size(); ++index)
		{
			auto& area = *mAreas[index];
			const bool visible = !area.GetTabs().empty();
			area.SetVisibility(visible ? UI::Visibility::Visible : UI::Visibility::Collapsed);
			auto rect = RegionBounds(static_cast<EditorDockRegion>(index), false);
			rect.x += 3;
			rect.y += 3;
			rect.Width = std::max(0.f, rect.Width - 6);
			rect.Height = std::max(0.f, rect.Height - 6);
			area.Measure({rect.Width, rect.Height});
			area.Arrange(rect);
		}
		if (mPreview >= 0 && mPreview < 5)
		{
			const auto rect = RegionBounds(static_cast<EditorDockRegion>(mPreview), true);
			pPreview->Measure({rect.Width, rect.Height});
			pPreview->Arrange(rect);
		}
	}

	UI::EventReply EditorDockLayout::OnInput(const UI::InputEvent& event)
	{
		using Type = UI::InputEvent::Type;
		if (event.EventType == Type::Cancel ||
			(event.EventType == Type::KeyDown && event.Key == Platform::Input::KEYCODE_ESCAPE))
		{
			if (mSplitter >= 0)
			{
				mLeft = mOriginal[0];
				mRight = mOriginal[1];
				mBottom = mOriginal[2];
				mBottomSplit = mOriginal[3];
				mSplitter = -1;
				InvalidateLayout();
			}
			return {true, false, true};
		}
		const auto bounds = GetBounds();
		if (event.EventType == Type::PointerDown && event.Button == MOUSE_BUTTON_LEFT)
		{
			const auto center = RegionBounds(EditorDockRegion::Documents, false);
			const auto right = RegionBounds(EditorDockRegion::Right, false);
			const auto bottomLeft = RegionBounds(EditorDockRegion::BottomLeft, false);
			if (!mAreas[0]->GetTabs().empty() && event.Position.y < center.y + center.Height &&
				std::abs(event.Position.x - center.x) < 5)
				mSplitter = 0;
			else if (!mAreas[2]->GetTabs().empty() && std::abs(event.Position.x - right.x) < 5)
				mSplitter = 1;
			else if ((!mAreas[3]->GetTabs().empty() || !mAreas[4]->GetTabs().empty()) &&
				event.Position.x < right.x && std::abs(event.Position.y - bottomLeft.y) < 5)
				mSplitter = 2;
			else if (!mAreas[3]->GetTabs().empty() && !mAreas[4]->GetTabs().empty() &&
				event.Position.y > bottomLeft.y && std::abs(event.Position.x - bottomLeft.x - bottomLeft.Width) < 5)
				mSplitter = 3;
			if (mSplitter >= 0)
			{
				mOriginal = {mLeft, mRight, mBottom, mBottomSplit};
				return {true, true, false, true};
			}
		}
		if (event.EventType == Type::PointerMove && mSplitter >= 0 && bounds.Width > 0 && bounds.Height > 0)
		{
			if (mSplitter == 0)
				mLeft = std::clamp((event.Position.x - bounds.x) / bounds.Width, .12f, .32f);
			if (mSplitter == 1)
				mRight = std::clamp(1 - (event.Position.x - bounds.x) / bounds.Width, .18f, .36f);
			if (mSplitter == 2)
				mBottom = std::clamp(1 - (event.Position.y - bounds.y) / bounds.Height, .20f, .60f);
			if (mSplitter == 3)
				mBottomSplit = std::clamp((event.Position.x - bounds.x) / (bounds.Width * (1 - mRight)), .3f, .75f);
			InvalidateLayout();
			return {true};
		}
		if (event.EventType == Type::PointerUp && event.Button == MOUSE_BUTTON_LEFT && mSplitter >= 0)
		{
			mSplitter = -1;
			return {true, false, true};
		}
		return {};
	}

	void EditorDockLayout::OnPaint(UI::DrawList& list, UI::Rect clip) const
	{
		list.Add(GetBounds(), clip, {.028f, .035f, .045f, 1});
	}
}
