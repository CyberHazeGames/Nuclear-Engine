#pragma once

#include <UI/ContentWidget.h>

namespace Nuclear::UI
{
	/// Single-child sizing wrapper; use inherited SetSizeLimits to constrain desired size.
	class NEAPI SizeBox : public ContentWidget
	{
	public:
		/// Creates an empty single-child sizing wrapper.
		SizeBox();
	};
} // namespace Nuclear::UI
