#include <AssetGridView.h>

namespace Nuclear::Editor
{
	void AssetGridView::OnArrange(UI::Rect content)
	{
		UI::ScrollBox::OnMeasure({content.Width, content.Height});
		UI::ScrollBox::OnArrange(content);
	}
}
