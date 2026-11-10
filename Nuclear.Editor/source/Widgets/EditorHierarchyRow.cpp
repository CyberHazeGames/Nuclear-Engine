#include <EditorHierarchyRow.h>
#include <Nuclear.Platform/Input.h>

namespace Nuclear::Editor
{
	EditorHierarchyRow::EditorHierarchyRow()
	{
		SetPadding({10, 7, 10, 7});
		UI::ControlStyle style;
		style.Normal = {.1f, .13f, .16f, 1};
		style.Hovered = {.075f, .15f, .18f, 1};
		style.Pressed = {.03f, .24f, .24f, 1};
		style.Focus = {.15f, .7f, .6f, 1};
		SetStyle(style);
	}

	UI::EventReply EditorHierarchyRow::OnInput(const UI::InputEvent& event)
	{
		if (event.EventType == UI::InputEvent::Type::PointerDown && event.Button == MOUSE_BUTTON_RIGHT)
		{
			auto callback = OnContextRequested;
			if (callback && GetContext()) callback(*GetContext(), event.Position);
			return {true};
		}
		return UI::Button::OnInput(event);
	}
}
