#pragma once

#include <UI/DrawList.h>

namespace Nuclear::UI::Detail
{
	/// Clamps finite values to [0, 1]; nonfinite values become zero.
	float UnitValue(float value);
	/// Returns the borrowed shader view only for a created brush texture.
	Diligent::ITextureView* BrushTexture(const Brush& brush);
	/// Emits the four rectangles forming a two-unit keyboard focus outline.
	void FocusOutline(DrawList& list, Rect bounds, Rect clip, Color color);
} // namespace Nuclear::UI::Detail
