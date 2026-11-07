#pragma once

#include <UI/Widget.h>

namespace Nuclear::UI
{
	/// Read-only horizontal normalized progress indicator.
	class NEAPI ProgressBar : public Widget
	{
	public:
		void SetValue(float value); ///< Clamps to [0, 1]; nonfinite values become zero.
		/// Normalized progress.
		float GetValue() const
		{
			return mValue;
		}
		void SetColors(Color background, Color fill); ///< Copies the track and fill colors.
	protected:
		/// Returns the default 180x18 logical-unit desired size.
		Vec2 OnMeasure(Vec2) override
		{
			return {180, 18};
		}
		void OnPaint(DrawList& list, Rect clip) const override;

	private:
		float mValue = 0; ///< Normalized fill fraction.
		Color mBackground = {.08f, .1f, .15f, 1}; ///< Empty track.
		Color mFill = {.1f, .65f, .45f, 1}; ///< Filled region.
	};
} // namespace Nuclear::UI
