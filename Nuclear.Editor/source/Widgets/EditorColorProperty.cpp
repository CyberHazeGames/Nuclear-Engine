#include <EditorColorProperty.h>
#include <EditorColorPickerWindow.h>
#include <EditorWidgets.h>

namespace Nuclear::Editor
{
	EditorColorProperty::EditorColorProperty(EditorColorPickerWindow& windows, const std::string& title,
		Math::Vector3 color, std::function<void(Math::Vector3)> committed, float maximum)
	{
		Build(windows, title, {color, 1}, false, maximum, [committed = std::move(committed)](Math::Vector4 value) {
			if (committed) committed(Math::Vector3(value));
		});
	}

	EditorColorProperty::EditorColorProperty(EditorColorPickerWindow& windows, const std::string& title,
		Math::Vector4 color, std::function<void(Math::Vector4)> committed, float maximum)
	{
		Build(windows, title, color, true, maximum, std::move(committed));
	}

	void EditorColorProperty::Build(EditorColorPickerWindow& windows, const std::string& title,
		Math::Vector4 color, bool alpha, float maximum, std::function<void(Math::Vector4)> committed)
	{
		SetPadding({8, 6, 8, 6});
		auto& row = Add<UI::HorizontalBox>();
		row.SetSpacing(10);
		auto& swatch = row.Add<UI::Border>();
		swatch.SetSizeLimits({48, 24}, {48, 24});
		// Keep the property visible even for transparent colors; opacity is inspected in the chooser.
		swatch.SetBrush({nullptr, {color.r, color.g, color.b, 1}});
		auto& label = Label(row, title + "...", 13);
		Fill(row, label);
		auto slot = row.GetSlot(label.GetId());
		slot.Vertical = UI::Alignment::Center;
		row.SetSlot(label.GetId(), slot);
		auto current = std::make_shared<Math::Vector4>(color);
		auto apply = [current, &swatch, committed = std::move(committed)](Math::Vector4 value) {
			*current = value;
			swatch.SetBrush({nullptr, {value.r, value.g, value.b, 1}});
			if (committed) committed(value);
		};
		OnClicked = [this, &windows, title, current, alpha, maximum, apply]() {
			windows.Open(*this, mLifetime, title, *current, alpha, maximum, apply);
		};
	}
}
