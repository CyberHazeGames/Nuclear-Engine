#pragma once

#include <UI/UserWidget.h>

/// Reusable C++ widget composition, built when attached to a viewport.
class UIShowcaseWidget final : public Nuclear::UI::UserWidget
{
protected:
	/// Builds the basic-widget demonstration tree on first attachment.
	void Construct() override;

private:
	int mClicks = 0; ///< Demonstrates retained instance state and callback binding.
};
