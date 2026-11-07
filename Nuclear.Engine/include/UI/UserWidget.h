#pragma once

#include <UI/ContentWidget.h>

namespace Nuclear::UI
{
	/// C++ composite widget. Override Construct() to build its single content subtree.
	class NEAPI UserWidget : public ContentWidget
	{
	protected:
		void OnAttached() override;
		virtual void Construct() {} ///< Called once on first attachment.
	private:
		bool mConstructed = false; ///< Prevents rebuilding on reattachment.
	};
} // namespace Nuclear::UI
