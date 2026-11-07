#pragma once

#include <UI/PanelWidget.h>

namespace Nuclear::UI
{
	/// Positions children through normalized anchors, offsets, pivot, and optional auto sizing.
	class NEAPI CanvasPanel : public PanelWidget
	{
	protected:
		void OnArrange(Rect content) override;
	};
} // namespace Nuclear::UI
