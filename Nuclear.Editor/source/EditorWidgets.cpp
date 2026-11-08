#include <EditorWidgets.h>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace Nuclear::Editor
{
	std::string Decimal(float value)
	{
		std::ostringstream stream;
		stream << std::fixed << std::setprecision(3) << value;
		return stream.str();
	}

	UI::TextBlock& Label(UI::PanelWidget& parent, const std::string& text, float size)
	{
		auto& label = parent.Add<UI::TextBlock>(text);
		label.SetFontSize(size);
		label.SetColor({.72f, .79f, .86f, 1});
		return label;
	}

	UI::Button& Action(UI::PanelWidget& parent, const std::string& text, std::function<void()> callback)
	{
		auto& button = parent.Add<UI::Button>();
		button.SetPadding({10, 7, 10, 7});
		UI::ControlStyle style;
		style.Normal = {.1f, .13f, .16f, 1};
		style.Hovered = {.075f, .15f, .18f, 1};
		style.Pressed = {.03f, .24f, .24f, 1};
		style.Focus = {.15f, .7f, .6f, 1};
		button.SetStyle(style);
		Label(button, text);
		button.OnClicked = std::move(callback);
		return button;
	}

	void Fill(UI::PanelWidget& parent, UI::Widget& child, float weight)
	{
		auto slot = parent.GetSlot(child.GetId());
		slot.Fill = weight;
		parent.SetSlot(child.GetId(), slot);
	}

	UI::VerticalBox& Section(UI::PanelWidget& parent, const std::string& title)
	{
		auto& border = parent.Add<UI::Border>();
		border.SetBrush({nullptr, {.06f, .075f, .095f, 1}});
		border.SetPadding({14, 12, 14, 12});
		auto& column = border.Add<UI::VerticalBox>();
		column.SetSpacing(10);
		auto& heading = Label(column, title, 14);
		heading.SetColor({.18f, .7f, .62f, 1});
		return column;
	}

	void Number(UI::PanelWidget& parent, const std::string& title, float value,
				std::function<void(float)> setter, float minimum, float maximum)
	{
		Label(parent, title, 13);
		auto& input = parent.Add<UI::TextBox>(Decimal(value));
		input.OnCommitted = [&input, setter = std::move(setter), minimum, maximum](const std::string& text) {
			try
			{
				size_t count = 0;
				float parsed = std::stof(text, &count);
				if (count != text.size() || !std::isfinite(parsed) || parsed < minimum || parsed > maximum)
					throw std::invalid_argument("Numeric field out of range");
				setter(parsed);
				input.SetText(Decimal(parsed));
			}
			catch (const std::exception&)
			{
				input.SetText("Invalid value");
			}
		};
	}
} // namespace Nuclear::Editor
