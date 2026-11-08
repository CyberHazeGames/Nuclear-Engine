#pragma once
#include <functional>
class NuclearEditor;
namespace Nuclear::Editor::Tests
{
	/// Exercises document tabs, docking transfers and secondary native UI windows in an initialized editor.
	void RunWorkspaceTests(NuclearEditor& editor, const std::function<void()>& frame);
}
