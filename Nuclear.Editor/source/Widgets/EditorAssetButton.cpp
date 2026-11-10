#include <EditorAssetButton.h>
#include <Nuclear.Platform/Input.h>
#include <cmath>
#include <utility>

namespace Nuclear::Editor
{
	EditorAssetButton::EditorAssetButton(DragCallback dragged, std::function<void()> cancelled)
		: mDragged(std::move(dragged)), mCancelled(std::move(cancelled)) {}

	void EditorAssetButton::CancelDrag()
	{
		const bool notify = std::exchange(mDragging, false);
		mPressed = false;
		if (notify && mCancelled) mCancelled();
	}

	void EditorAssetButton::OnRemoved()
	{
		CancelDrag();
	}

	UI::EventReply EditorAssetButton::OnInput(const UI::InputEvent& event)
	{
		using Type = UI::InputEvent::Type;
		if (event.EventType == Type::Cancel ||
			(event.EventType == Type::KeyDown && event.Key == Platform::Input::KEYCODE_ESCAPE))
		{
			CancelDrag();
			UI::Button::OnInput({Type::Cancel});
			return {true, false, true};
		}
		if (event.EventType == Type::PointerDown && event.Button == MOUSE_BUTTON_LEFT)
		{
			mStart = event.Position;
			mPressed = true;
		}
		if (event.EventType == Type::PointerMove && mPressed)
		{
			if (!mDragging && std::hypot(event.Position.x - mStart.x, event.Position.y - mStart.y) > 6)
			{
				mDragging = true;
				// Cancel the click's pressed appearance, preserving this widget's context capture.
				UI::Button::OnInput({Type::Cancel});
			}
			if (mDragging && mDragged && GetContext()) mDragged(*GetContext(), event.Position, false);
			return {true};
		}
		if (event.EventType == Type::PointerUp && event.Button == MOUSE_BUTTON_LEFT)
		{
			mPressed = false;
			if (std::exchange(mDragging, false))
			{
				auto callback = mDragged;
				if (callback && GetContext()) callback(*GetContext(), event.Position, true);
				return {true, false, true};
			}
		}
		return UI::Button::OnInput(event);
	}
}
