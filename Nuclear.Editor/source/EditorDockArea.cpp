#include <EditorDockArea.h>
#include <UI/DrawList.h>
#include <algorithm>

namespace Nuclear::Editor
{
	EditorDockArea::EditorDockArea()
	{
		pStrip = &Add<EditorTabStrip>();
	}

	void EditorDockArea::ShowHeader(bool show)
	{
		mShowHeader = show;
		pStrip->SetVisibility(show ? UI::Visibility::Visible : UI::Visibility::Collapsed);
		InvalidateLayout();
	}

	void EditorDockArea::AddTab(EditorTabStrip::Tab tab, std::unique_ptr<UI::Widget> content, size_t index)
	{
		index = std::min(index, mTabs.size());
		const auto id = content->GetId();
		AddChild(std::move(content));
		mTabs.insert(mTabs.begin() + index, std::move(tab));
		mContents.insert(mContents.begin() + index, id);
		Activate(mTabs[index].Id);
	}

	std::unique_ptr<UI::Widget> EditorDockArea::TakeTab(const std::string& id)
	{
		for (size_t index = 0; index < mTabs.size(); ++index)
		{
			if (mTabs[index].Id != id)
				continue;
			auto content = TakeChild(mContents[index]);
			mTabs.erase(mTabs.begin() + index);
			mContents.erase(mContents.begin() + index);
			if (id == mActive)
				mActive = mTabs.empty() ? "" : mTabs[std::min(index, mTabs.size() - 1)].Id;
			Activate(mActive);
			return content;
		}
		return nullptr;
	}

	void EditorDockArea::Activate(const std::string& id)
	{
		mActive = id;
		for (size_t index = 0; index < mContents.size(); ++index)
		{
			for (auto& child : mChildren)
				if (child.Child->GetId() == mContents[index])
					child.Child->SetVisibility(mTabs[index].Id == id ? UI::Visibility::Visible : UI::Visibility::Collapsed);
		}
		pStrip->SetTabs(mTabs, mActive);
		InvalidateLayout();
		if (OnChanged)
			OnChanged();
	}

	void EditorDockArea::SetTabTitle(const std::string& id, const std::string& title)
	{
		for (auto& tab : mTabs)
			if (tab.Id == id)
				tab.Title = title;
		pStrip->SetTabs(mTabs, mActive);
		if (OnChanged)
			OnChanged();
	}

	UI::Vec2 EditorDockArea::OnMeasure(UI::Vec2 available)
	{
		PanelWidget::OnMeasure(available);
		return {320, 220};
	}

	void EditorDockArea::OnArrange(UI::Rect content)
	{
		const float header = mShowHeader ? std::min(34.f, content.Height) : 0;
		pStrip->Measure({content.Width, header});
		pStrip->Arrange({content.x, content.y, content.Width, header});
		for (auto& entry : mChildren)
		{
			if (entry.Child.get() == pStrip)
				continue;
			entry.Child->Measure({content.Width, std::max(0.f, content.Height - header)});
			entry.Child->Arrange({content.x + 1, content.y + header, std::max(0.f, content.Width - 2),
				std::max(0.f, content.Height - header - 1)});
		}
	}

	void EditorDockArea::OnPaint(UI::DrawList& list, UI::Rect clip) const
	{
		list.Add(GetBounds(), clip, {.055f, .07f, .085f, 1});
	}
}
