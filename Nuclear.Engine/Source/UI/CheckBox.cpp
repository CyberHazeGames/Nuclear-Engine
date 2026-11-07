#include <UI/CheckBox.h>
#include <UI/DrawList.h>

namespace Nuclear::UI
{
	CheckBox::CheckBox()
	{
		SetPadding({36, 8, 12, 8});
	}

	void CheckBox::SetChecked(bool checked)
	{
		mChecked = checked;
	}

	void CheckBox::Activate()
	{
		mChecked = !mChecked;
		auto callback = OnChanged;
		if (callback)
			callback(mChecked);
		Button::Activate();
	}

	void CheckBox::OnPaint(DrawList& list, Rect clip) const
	{
		Button::OnPaint(list, clip);
		auto bounds = GetBounds();
		Rect box = {bounds.x + 10, bounds.y + (bounds.Height - 16) * .5f, 16, 16};
		list.Add(box, clip, {.04f, .05f, .08f, 1});
		if (mChecked)
			list.Add({box.x + 3, box.y + 3, 10, 10}, clip, {.2f, .8f, .55f, 1});
	}
} // namespace Nuclear::UI
