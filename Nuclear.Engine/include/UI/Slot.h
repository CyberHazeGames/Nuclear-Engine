#pragma once

#include <UI/Types.h>

namespace Nuclear::UI
{
	/// Shared slot properties. Canvas offsets use Width/Height as size when not stretched,
	/// and as right/bottom margins when the corresponding anchors are stretched.
	struct Slot
	{
		Margin Padding; ///< Outside child padding in logical units.
		Alignment Horizontal = Alignment::Fill; ///< Alignment within allocated space.
		Alignment Vertical = Alignment::Fill; ///< Alignment within allocated space.
		float Fill = 0; ///< Positive weight shares remaining box space; zero uses desired size.
		Vec2 AnchorMin; ///< Canvas start anchors in [0, 1].
		Vec2 AnchorMax; ///< Canvas end anchors in [0, 1], not below AnchorMin.
		Rect Offsets = {0, 0, 100, 30}; ///< Canvas position/size or stretched edge margins.
		Vec2 Pivot; ///< Canvas alignment fraction for fixed-size axes.
		int ZOrder = 0; ///< Higher values paint later; equal values preserve insertion order.
		bool AutoSize = false; ///< Fixed canvas axes use desired size instead of Offsets size.
	};
} // namespace Nuclear::UI
