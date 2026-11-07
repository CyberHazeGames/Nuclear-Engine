#pragma once

#include <UI/Types.h>

namespace Nuclear::UI
{
	/// Value-based backgrounds for an interactive control.
	struct ControlStyle
	{
		Color Normal = {.12f, .16f, .23f, 1}; ///< Idle background.
		Color Hovered = {.2f, .28f, .4f, 1}; ///< Pointer-hover background.
		Color Pressed = {.08f, .3f, .55f, 1}; ///< Active press background.
		Color Disabled = {.12f, .12f, .12f, .6f}; ///< Disabled background.
		Color Focus = {.3f, .7f, 1, 1}; ///< Keyboard-focus outline.
	};
} // namespace Nuclear::UI
