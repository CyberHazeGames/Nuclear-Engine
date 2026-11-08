#pragma once
#include <functional>
class NuclearEditor;

namespace Nuclear::Editor::Tests
{
	/// Exercises scene property commits/history, clear-target pixels, docking, modal isolation and scene reset.
	void RunSceneSettingsTests(NuclearEditor& editor, const std::function<void()>& frame);
}
