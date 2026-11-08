#pragma once
#include <UI/UI.h>
#include <Math/Math.h>
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
	/// Adds a labeled row of three axis-colored scalar editors for a 3D vector.
	/// A fixed-width SizeBox wraps the title label so multiple vector rows align vertically.
	/// The setter callback receives the modified axis index (0 for X, 1 for Y, 2 for Z) and the validated value.
	/// Values outside [minimum, maximum] or non-finite inputs are rejected on Enter commit.
	void Vector(UI::PanelWidget& parent, const std::string& title, Math::Vector3 value,
				std::function<void(int, float)> setter, float minimum = -100000, float maximum = 100000,
				float labelWidth = 80.0f);
	/// Adds an interactive asset tile button displaying a centered thumbnail image and label underneath,
	/// matching Unreal Engine Content Browser and desktop icon layouts.
	/// The button owns an internal vertical layout containing the thumbnail brush and text labels.
	/// Clicking the tile button executes the callback (typically opening the corresponding asset document).
	/// The returned button reference is owned by parent.
	UI::Button& AssetTile(UI::PanelWidget& parent, const std::string& name,
						  const UI::Brush& brush, UI::Vec2 imageSize,
						  std::function<void()> callback, const std::string& subtext = {});
	/// Formats a compact number with three fractional digits.
	std::string Decimal(float value);
} // namespace Nuclear::Editor
