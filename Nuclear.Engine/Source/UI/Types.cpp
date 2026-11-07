#include <UI/Types.h>

namespace Nuclear::UI
{
	Rect Rect::Intersect(const Rect& other) const
	{
		const float left = std::max(x, other.x);
		const float top = std::max(y, other.y);
		return {left, top, std::max(0.0f, std::min(x + Width, other.x + other.Width) - left),
				std::max(0.0f, std::min(y + Height, other.y + other.Height) - top)};
	}
} // namespace Nuclear::UI
