#include "WidgetUtilities.h"
#include <Assets/Texture.h>
#include <cmath>

namespace Nuclear::UI::Detail
{
	float UnitValue(float value)
	{
		return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0;
	}
	Diligent::ITextureView* BrushTexture(const Brush& brush)
	{
		return brush.pTexture && brush.pTexture->GetState() == Assets::IAsset::State::Created
				   ? brush.pTexture->GetTextureView()
				   : nullptr;
	}
	void FocusOutline(DrawList& list, Rect bounds, Rect clip, Color color)
	{
		list.Add({bounds.x, bounds.y, bounds.Width, 2}, clip, color);
		list.Add({bounds.x, bounds.y + bounds.Height - 2, bounds.Width, 2}, clip, color);
		list.Add({bounds.x, bounds.y, 2, bounds.Height}, clip, color);
		list.Add({bounds.x + bounds.Width - 2, bounds.y, 2, bounds.Height}, clip, color);
	}
} // namespace Nuclear::UI::Detail
