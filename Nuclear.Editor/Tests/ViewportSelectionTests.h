#pragma once

#include <functional>

class NuclearEditor;

namespace Nuclear::Editor::Tests
{
	/// Exercises visible-mesh picking, hierarchy/inspector synchronization, filtering,
	/// yellow silhouette pixels, occlusion and fractional UI scaling in a loaded editor.
	void RunViewportSelectionTests(NuclearEditor& editor, const std::function<void()>& frame);
}
