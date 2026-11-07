#pragma once

#include <UI/Widget.h>

namespace Nuclear::UI
{
	/// Tinted image. A null/uncreated texture draws a solid tint instead.
	class NEAPI Image : public Widget
	{
	public:
		/// Copies borrowed texture and tint.
		void SetBrush(const Brush& brush)
		{
			mBrush = brush;
		}
		/// Desired logical size.
		void SetImageSize(Vec2 size)
		{
			mSize = size;
			InvalidateLayout();
		}

	protected:
		/// Returns the configured desired image size in logical units.
		Vec2 OnMeasure(Vec2) override
		{
			return mSize;
		}
		void OnPaint(DrawList& list, Rect clip) const override;

	private:
		Brush mBrush; ///< Texture, UVs, and tint.
		Vec2 mSize = {64, 64}; ///< Desired size.
	};
} // namespace Nuclear::UI
