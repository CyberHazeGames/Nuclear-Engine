#pragma once
#include <UI/Widget.h>
#include <memory>
#include <string>

namespace Nuclear::Editor
{
	/// Builds the About Nuclear Engine document window.
	/// Displays engine build metadata, editor build metadata, official repository links, and licensing information.
	class AboutDocument
	{
	public:
		/// Returns the document tab title ("About Nuclear Engine").
		static std::string GetTitle();

		/// Builds the scrollable about view with build information, repository link, and license attributions.
		static std::unique_ptr<UI::Widget> Build();
	};
} // namespace Nuclear::Editor
