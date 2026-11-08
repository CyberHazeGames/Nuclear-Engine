#pragma once
#include <UI/UI.h>
#include <functional>
#include <string>

namespace Nuclear::Editor
{
	/// Adds a compact editor label; the panel owns the returned reference.
	UI::TextBlock& Label(UI::PanelWidget& parent, const std::string& text, float size = 16);
	/// Adds a retained action button; callbacks run on the window thread.
	UI::Button& Action(UI::PanelWidget& parent, const std::string& text, std::function<void()> callback);
	/// Marks a box child as sharing remaining space with the supplied positive weight.
	void Fill(UI::PanelWidget& parent, UI::Widget& child, float weight = 1);
	/// Creates a padded, dark section containing a heading and an owning content column.
	UI::VerticalBox& Section(UI::PanelWidget& parent, const std::string& title);
	/// Adds a finite scalar editor; Enter commits only valid values within inclusive limits.
	void Number(UI::PanelWidget& parent, const std::string& title, float value,
				std::function<void(float)> setter, float minimum = -100000, float maximum = 100000);
	/// Formats a compact number with three fractional digits.
	std::string Decimal(float value);
} // namespace Nuclear::Editor
