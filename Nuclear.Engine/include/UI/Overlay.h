#pragma once

#include <UI/PanelWidget.h>

namespace Nuclear::UI
{
	/// Overlays children using their slot alignment and stable Z-order.
	class NEAPI Overlay : public PanelWidget
	{
	public:
		/// Creates an empty overlay panel.
		Overlay();
	};
} // namespace Nuclear::UI
