#pragma once

#include <UI/ContentWidget.h>

namespace Nuclear::UI
{
	/// Single-child background container.
	class NEAPI Border : public ContentWidget
	{
	public:
		/// Copies a borrowed-resource brush.
		void SetBrush(const Brush& brush)
		{
			mBrush = brush;
		}

	protected:
		void OnPaint(DrawList& list, Rect clip) const override;

	private:
		Brush mBrush; ///< Background style; solid white by default.
	};
} // namespace Nuclear::UI
