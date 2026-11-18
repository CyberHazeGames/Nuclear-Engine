#pragma once
#include <UI/ScrollBox.h>

namespace Nuclear::Editor
{
	/// Vertical viewport for a wrapping asset grid in a resizable dock or detached panel.
	class AssetGridView : public UI::ScrollBox
	{
	protected:
		/// Measures the rows at the allocated pane width before calculating the scroll range.
		void OnArrange(UI::Rect content) override;
	};
}
