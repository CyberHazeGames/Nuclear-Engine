#include <UI/UserWidget.h>

namespace Nuclear::UI
{
	void UserWidget::OnAttached()
	{
		if (!mConstructed)
		{
			mConstructed = true;
			Construct();
		}
	}
} // namespace Nuclear::UI
