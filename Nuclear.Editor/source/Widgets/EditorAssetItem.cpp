#include <EditorAssetItem.h>
#include <UI/UIContext.h>

namespace Nuclear::Editor
{
	EditorAssetItem::EditorAssetItem(EditorMenuPopup::Items commands) : mCommands(std::move(commands))
	{
		SetBrush({nullptr, {0, 0, 0, 0}});
	}

	void EditorAssetItem::CloseMenu()
	{
		if (mPopup && GetContext())
			GetContext()->RemoveFromViewport(std::exchange(mPopup, 0));
	}

	void EditorAssetItem::OnRemoved()
	{
		CloseMenu();
	}

	UI::EventReply EditorAssetItem::OnInput(const UI::InputEvent& event)
	{
		if (event.EventType != UI::InputEvent::Type::PointerDown || event.Button != MOUSE_BUTTON_RIGHT)
			return {};

		CloseMenu();
		auto* context = GetContext();
		if (!context)
			return {};
		auto popup = std::make_unique<EditorMenuPopup>(
			UI::Rect{event.Position.x, event.Position.y, 0, 0}, mCommands,
			[this]() { CloseMenu(); },
			[this](UI::Vec2, bool clicked) { if (clicked) CloseMenu(); },
			[](int) {});
		mPopup = popup->GetId();
		context->AddToViewport(std::move(popup), 60);
		context->SetFocus(mPopup);
		return {true};
	}
}
