#pragma once

#include <UI/PanelWidget.h>

namespace Nuclear::UI
{
	/// Owning panel limited to one child; SetContent replaces any previous child.
	class NEAPI ContentWidget : public PanelWidget
	{
	public:
		/// Adds the sole content child; throws if content already exists.
		Widget& AddChild(std::unique_ptr<Widget> child, const Slot& slot = {}) override;
		/// Replaces content outside dispatch; replacement during dispatch runs after bubbling.
		void SetContent(std::unique_ptr<Widget> child);
	};
} // namespace Nuclear::UI
