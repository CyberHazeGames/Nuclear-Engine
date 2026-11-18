#include <EditorWidgets.h>
#include <Assets/Texture.h>
#include <EditorSettings.h>
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
		heading.SetColor(EditorSettings::Get().GetAccentColor());
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

	void Vector(UI::PanelWidget& parent, const std::string& title, Math::Vector3 value,
				std::function<void(int, float)> setter, float minimum, float maximum,
				float labelWidth)
	{
		auto& row = parent.Add<UI::HorizontalBox>();
		row.SetSpacing(6);

		// Fixed-width container for the title label so adjacent vector rows align vertically.
		auto& labelBox = row.Add<UI::SizeBox>();
		labelBox.SetSizeLimits({labelWidth, 0}, {labelWidth, 1000000});
		Label(labelBox, title, 12);
		auto groupSlot = row.GetSlot(labelBox.GetId());
		groupSlot.Vertical = UI::Alignment::Center;
		row.SetSlot(labelBox.GetId(), groupSlot);

		for (int axis = 0; axis < 3; ++axis)
		{
			// Axis label with conventional representation colors (X=Red, Y=Green, Z=Blue).
			auto& axisLabel = Label(row, std::string(1, "XYZ"[axis]), 13);
			if (axis == 0)
				axisLabel.SetColor({.85f, .25f, .25f, 1});
			else if (axis == 1)
				axisLabel.SetColor({.25f, .75f, .25f, 1});
			else
				axisLabel.SetColor({.35f, .55f, .95f, 1});

			auto axisSlot = row.GetSlot(axisLabel.GetId());
			axisSlot.Vertical = UI::Alignment::Center;
			row.SetSlot(axisLabel.GetId(), axisSlot);

			// Value field sharing remaining horizontal space equally.
			auto& input = row.Add<UI::TextBox>(Decimal(value[axis]));
			Fill(row, input);
			input.OnCommitted = [&input, setter, axis, minimum, maximum](const std::string& text) {
				try
				{
					size_t count = 0;
					float parsed = std::stof(text, &count);
					if (count != text.size() || !std::isfinite(parsed) || parsed < minimum || parsed > maximum)
						throw std::invalid_argument("Numeric field out of range");
					setter(axis, parsed);
					input.SetText(Decimal(parsed));
				}
				catch (const std::exception&)
				{
					input.SetText("Invalid value");
				}
			};
		}
	}

	UI::HorizontalBox& SegmentedButton(UI::PanelWidget& parent, const std::string& title,
									   const std::vector<std::string>& options,
									   size_t selectedIndex,
									   std::function<void(size_t)> onSelectionChanged,
									   float labelWidth)
	{
		auto& row = parent.Add<UI::HorizontalBox>();
		row.SetSpacing(6);

		if (!title.empty())
		{
			auto& labelBox = row.Add<UI::SizeBox>();
			labelBox.SetSizeLimits({labelWidth, 0}, {labelWidth, 1000000});
			Label(labelBox, title, 12);
			auto groupSlot = row.GetSlot(labelBox.GetId());
			groupSlot.Vertical = UI::Alignment::Center;
			row.SetSlot(labelBox.GetId(), groupSlot);
		}

		auto& border = row.Add<UI::Border>();
		border.SetBrush({nullptr, {.06f, .075f, .095f, 1.0f}});
		border.SetPadding({2, 2, 2, 2});
		Fill(row, border, 1.0f);

		auto& segmentBox = border.Add<UI::HorizontalBox>();
		segmentBox.SetSpacing(2);

		UI::ControlStyle selectedStyle;
		selectedStyle.Normal = {.16f, .50f, .45f, 1.0f};
		selectedStyle.Hovered = {.20f, .58f, .52f, 1.0f};
		selectedStyle.Pressed = {.12f, .42f, .38f, 1.0f};
		selectedStyle.Focus = {.25f, .80f, .70f, 1.0f};

		UI::ControlStyle unselectedStyle;
		unselectedStyle.Normal = {.095f, .125f, .16f, 1.0f};
		unselectedStyle.Hovered = {.14f, .19f, .24f, 1.0f};
		unselectedStyle.Pressed = {.075f, .10f, .13f, 1.0f};
		unselectedStyle.Focus = {.20f, .60f, .55f, 1.0f};

		struct SegmentItem
		{
			UI::Button* pButton = nullptr;
			UI::TextBlock* pLabel = nullptr;
		};
		auto items = std::make_shared<std::vector<SegmentItem>>();
		items->reserve(options.size());

		for (size_t i = 0; i < options.size(); ++i)
		{
			const bool isSelected = (i == selectedIndex);
			auto& button = segmentBox.Add<UI::Button>();
			button.SetPadding({8, 5, 8, 5});
			button.SetStyle(isSelected ? selectedStyle : unselectedStyle);
			Fill(segmentBox, button, 1.0f);

			auto& label = Label(button, options[i], 12);
			label.SetColor(isSelected ? UI::Color{.95f, 1.0f, 1.0f, 1.0f} : UI::Color{.62f, .70f, .78f, 1.0f});
			auto labelSlot = button.GetSlot(label.GetId());
			labelSlot.Horizontal = UI::Alignment::Center;
			labelSlot.Vertical = UI::Alignment::Center;
			button.SetSlot(label.GetId(), labelSlot);

			items->push_back({&button, &label});
		}

		for (size_t i = 0; i < options.size(); ++i)
		{
			auto* btn = (*items)[i].pButton;
			btn->OnClicked = [i, items, onSelectionChanged, selectedStyle, unselectedStyle]() {
				for (size_t j = 0; j < items->size(); ++j)
				{
					const bool sel = (j == i);
					(*items)[j].pButton->SetStyle(sel ? selectedStyle : unselectedStyle);
					(*items)[j].pLabel->SetColor(sel ? UI::Color{.95f, 1.0f, 1.0f, 1.0f} : UI::Color{.62f, .70f, .78f, 1.0f});
				}
				if (onSelectionChanged)
					onSelectionChanged(i);
			};
		}

		return row;
	}

	UI::Button& AssetTile(UI::PanelWidget& parent, const std::string& name,
						  const UI::Brush& brush, UI::Vec2 imageSize,
						  std::function<void()> callback, const std::string& subtext,
						  EditorAssetButton::DragCallback dragged, std::function<void()> cancelled,
						  std::unique_ptr<UI::Widget> icon, UI::Color accent)
	{
		UI::Button& button = dragged
			? parent.Add<EditorAssetButton>(std::move(dragged), std::move(cancelled))
			: parent.Add<UI::Button>();
		const float width = imageSize.x + 8;
		button.SetSizeLimits({width, 0}, {width, 10000});
		button.SetPadding({4, 4, 4, 3});
		UI::ControlStyle style;
		style.Normal = {.075f, .085f, .102f, 1};
		style.Hovered = {.13f, .18f, .25f, 1};
		style.Pressed = {.10f, .24f, .41f, 1};
		style.Focus = EditorSettings::Get().GetAccentColor();
		button.SetStyle(style);

		auto& tile = button.Add<UI::VerticalBox>();
		if (icon)
			tile.AddChild(std::move(icon));
		else
		{
			auto& well = tile.Add<UI::Border>();
			well.SetBrush({nullptr, {.048f, .056f, .069f, 1}});
			well.SetSizeLimits(imageSize, imageSize);
			auto& image = well.Add<UI::Image>();
			UI::Vec2 fitted = imageSize;
			if (brush.pTexture && brush.pTexture->GetWidth() && brush.pTexture->GetHeight())
			{
				const float scale = std::min(imageSize.x / brush.pTexture->GetWidth(), imageSize.y / brush.pTexture->GetHeight());
				fitted = {scale * brush.pTexture->GetWidth(), scale * brush.pTexture->GetHeight()};
			}
			image.SetBrush(brush);
			image.SetImageSize(fitted);
			auto slot = well.GetSlot(image.GetId());
			slot.Horizontal = slot.Vertical = UI::Alignment::Center;
			well.SetSlot(image.GetId(), slot);
		}
		auto& stripe = tile.Add<UI::Border>();
		stripe.SetBrush({nullptr, accent});
		stripe.SetSizeLimits({0, 2}, {10000, 2});
		auto& label = Label(tile, name, 12);
		label.SetWrap(true);
		label.SetPadding({2, 3, 2, 0});
		label.SetSizeLimits({imageSize.x, 32}, {imageSize.x, 32});
		if (!subtext.empty())
		{
			auto& sub = Label(tile, subtext, 10);
			sub.SetColor({.58f, .64f, .71f, 1});
			sub.SetPadding({2, 0, 0, 0});
		}

		button.OnClicked = std::move(callback);
		return button;
	}
} // namespace Nuclear::Editor
