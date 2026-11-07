#pragma once

#include <UI/PanelWidget.h>

namespace Nuclear::UI
{
	/// Shared sequential box layout; only positive slot Fill weights stretch the main axis.
	class NEAPI BoxPanel : public PanelWidget
	{
	public:
		/// Nonnegative space between noncollapsed children, in logical units.
		void SetSpacing(float spacing);

	protected:
		explicit BoxPanel(bool horizontal); ///< Selects the main axis.
		Vec2 OnMeasure(Vec2 available) override;
		void OnArrange(Rect content) override;

	private:
		bool mHorizontal; ///< True arranges left to right, false top to bottom.
		float mSpacing = 0; ///< Gap in logical units.
	};
} // namespace Nuclear::UI
