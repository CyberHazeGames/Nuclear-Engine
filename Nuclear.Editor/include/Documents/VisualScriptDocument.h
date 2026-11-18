#pragma once

#include <UI/VerticalBox.h>

namespace Nuclear::Editor
{
	/// Dockable authoring document containing a node palette, property inspector and graph canvas.
	class VisualScriptDocument : public UI::VerticalBox
	{
	public:
		/// Creates a self-contained example graph and wires the retained controls to it.
		VisualScriptDocument();
	};
}
