#include <EditorMenuPopup.h>
#include <EditorWidgets.h>
#include <Nuclear.Platform/Input.h>
#include <UI/UIContext.h>

namespace Nuclear::Editor
{
	EditorMenuPopup::EditorMenuPopup(UI::Rect anchor, const Items& items, std::function<void()> dismiss,
		std::function<void(UI::Vec2, bool)> navigate, std::function<void(int)> switchMenu)
		: mAnchor(anchor), mItems(items), mDismiss(std::move(dismiss)), mNavigate(std::move(navigate)),
		  mSwitchMenu(std::move(switchMenu))
	{
		for (const auto& [label, command] : items)
		{
			auto& button = Action(*this, label, [dismiss = mDismiss, command]() {
				dismiss();
				if (command) command();
			});
			button.SetEnabled(bool(command));
			UI::ControlStyle style;
			style.Normal = {.065f, .08f, .10f, 1};
			style.Hovered = {.08f, .22f, .24f, 1};
			style.Pressed = {.035f, .15f, .17f, 1};
			button.SetStyle(style);
		}
	}

	void EditorMenuPopup::OnArrange(UI::Rect content)
	{
		const float width = std::min(260.f, content.Width);
		const float height = std::min(float(mChildren.size()) * 36 + 8, content.Height);
		mPopup = {std::clamp(mAnchor.x, content.x, content.x + content.Width - width),
			std::clamp(mAnchor.y + mAnchor.Height, content.y, content.y + content.Height - height), width, height};
		float top = mPopup.y + 4;
		for (auto& child : mChildren)
		{
			child.Child->Arrange({mPopup.x + 4, top, width - 8, 36});
			top += 36;
		}
	}

	void EditorMenuPopup::OnPaint(UI::DrawList& list, UI::Rect clip) const
	{
		list.Add({mPopup.x + 3, mPopup.y + 3, mPopup.Width, mPopup.Height}, clip, {0, 0, 0, .35f});
		list.Add(mPopup, clip, {.18f, .23f, .27f, 1});
		list.Add({mPopup.x + 1, mPopup.y + 1, mPopup.Width - 2, mPopup.Height - 2}, clip, {.065f, .08f, .10f, 1});
	}

	UI::EventReply EditorMenuPopup::OnInput(const UI::InputEvent& event)
	{
		if (event.EventType == UI::InputEvent::Type::Cancel)
		{
			// Focus may be moving to a command child. Inspect it after the focus change
			// completes, resolving the popup by ID in case it was removed meanwhile.
			auto* context = GetContext();
			const auto id = GetId();
			context->Defer([context, id]() {
				auto* popup = dynamic_cast<EditorMenuPopup*>(context->Find(id));
				if (!popup)
					return;
				for (auto* focused = context->Find(context->GetFocusedWidget()); focused; focused = focused->GetParent())
					if (focused == popup)
						return;
				popup->mDismiss();
			});
			return {true};
		}
		if (event.EventType == UI::InputEvent::Type::KeyDown && !event.Repeat && !mItems.empty())
		{
			if (event.Key == Platform::Input::KEYCODE_LEFT || event.Key == Platform::Input::KEYCODE_RIGHT)
			{
				mSwitchMenu(event.Key == Platform::Input::KEYCODE_LEFT ? -1 : 1);
				return {true};
			}
			if (event.Key == Platform::Input::KEYCODE_DOWN || event.Key == Platform::Input::KEYCODE_UP)
			{
				const size_t step = event.Key == Platform::Input::KEYCODE_DOWN ? 1 : mItems.size() - 1;
				size_t next = mSelected == size_t(-1) ? (step == 1 ? mItems.size() - 1 : 0) : mSelected;
				for (size_t attempt = 0; attempt < mItems.size(); ++attempt)
				{
					next = (next + step) % mItems.size();
					if (mItems[next].second) { mSelected = next; break; }
				}
				for (size_t index = 0; index < mChildren.size(); ++index)
				{
					UI::ControlStyle style;
					style.Normal = index == mSelected ? UI::Color{.08f, .22f, .24f, 1} : UI::Color{.065f, .08f, .10f, 1};
					style.Hovered = {.08f, .22f, .24f, 1};
					static_cast<UI::Button*>(mChildren[index].Child.get())->SetStyle(style);
				}
				return {true};
			}
			if (event.Key == Platform::Input::KEYCODE_RETURN && mSelected < mItems.size())
			{
				auto command = mItems[mSelected].second;
				mDismiss();
				if (command) command();
				return {true};
			}
		}
		if (event.EventType == UI::InputEvent::Type::KeyDown && event.Key == Platform::Input::KEYCODE_ESCAPE)
		{
			mDismiss();
			return {true};
		}
		if (event.EventType == UI::InputEvent::Type::PointerMove && !mPopup.Contains(event.Position))
			mNavigate(event.Position, false);
		if (event.EventType == UI::InputEvent::Type::PointerDown && !mPopup.Contains(event.Position))
		{
			mNavigate(event.Position, true);
			return {true};
		}
		return {};
	}
}
