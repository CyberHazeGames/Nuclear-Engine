#include <AssetViewerLayout.h>
#include <UI/DrawList.h>
#include <Nuclear.Platform/Input.h>
#include <algorithm>

namespace Nuclear::Editor
{
	AssetViewerLayout::AssetViewerLayout()
	{
		pPreview = &Add<UI::ContentWidget>();
		pProperties = &Add<UI::ContentWidget>();
	}

	void AssetViewerLayout::OnArrange(UI::Rect content)
	{
		const float properties = std::min(content.Width * .5f, std::max(220.f, content.Width * mPropertiesRatio));
		const float divider = std::min(6.f, content.Width);
		const float preview = std::max(0.f, content.Width - properties - divider);
		mDivider = {content.x + preview, content.y, divider, content.Height};
		pPreview->Measure({preview, content.Height});
		pProperties->Measure({properties, content.Height});
		pPreview->Arrange({content.x, content.y, preview, content.Height});
		pProperties->Arrange({mDivider.x + divider, content.y, properties, content.Height});
	}

	void AssetViewerLayout::OnPaint(UI::DrawList& list, UI::Rect clip) const
	{
		list.Add(GetBounds(), clip, {.045f, .055f, .07f, 1});
		list.Add(mDivider, clip, mDragging ? UI::Color{.08f, .42f, .44f, 1} : UI::Color{.12f, .15f, .18f, 1});
	}

	UI::EventReply AssetViewerLayout::OnInput(const UI::InputEvent& event)
	{
		using Type = UI::InputEvent::Type;
		if (event.EventType == Type::Cancel ||
			(event.EventType == Type::KeyDown && event.Key == Platform::Input::KEYCODE_ESCAPE))
		{
			if (mDragging)
			{
				mPropertiesRatio = mOriginalRatio;
				mDragging = false;
				InvalidateLayout();
			}
			return {true, false, true};
		}
		if (event.EventType == Type::PointerDown && event.Button == MOUSE_BUTTON_LEFT && mDivider.Contains(event.Position))
		{
			mDragging = true;
			mOriginalRatio = mPropertiesRatio;
			return {true, true, false, true};
		}
		if (event.EventType == Type::PointerMove && mDragging && GetBounds().Width > 0)
		{
			mPropertiesRatio = std::clamp(1 - (event.Position.x - GetBounds().x) / GetBounds().Width, .2f, .5f);
			InvalidateLayout();
			return {true};
		}
		if (event.EventType == Type::PointerUp && event.Button == MOUSE_BUTTON_LEFT && mDragging)
		{
			mDragging = false;
			return {true, false, true};
		}
		return {};
	}
}
