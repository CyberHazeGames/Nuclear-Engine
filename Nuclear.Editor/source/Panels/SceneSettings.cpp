#include <SceneSettings.h>
#include <EditorHistory.h>
#include <EditorWidgets.h>

namespace Nuclear::Editor
{
	SceneSettings::SceneSettings(EditorHistory& history, std::function<void()> changed)
		: mHistory(history), mChanged(std::move(changed))
	{
		Refresh();
	}

	void SceneSettings::Refresh()
	{
		auto root = std::make_unique<UI::VerticalBox>();
		root->SetPadding({12, 10, 12, 10});
		root->SetSpacing(12);
		const auto state = EditorSceneState::Capture();
		if (!state.IsValid())
		{
			Label(*root, "No active scene.");
			SetContent(std::move(root));
			return;
		}
		Label(*root, "SCENE SETTINGS", 18);
		Label(*root, "Scene name", 13);
		auto& name = root->Add<UI::TextBox>(state.Name);
		name.OnCommitted = [this](const std::string& value) {
			mHistory.EditScene("Rename scene", [&]() {
				auto current = EditorSceneState::Capture();
				const auto first = value.find_first_not_of(" \t\r\n");
				current.Name = first == std::string::npos ? "Untitled Scene"
					: value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
				current.Restore();
			});
			mChanged();
		};
		auto& color = Section(*root, "CLEAR COLOR");
		Label(color, "Linear RGBA, from 0 to 1. Enter commits each value.", 12).SetWrap(true);
		auto& preview = color.Add<UI::Border>();
		preview.SetBrush({nullptr, {state.ClearColor.r, state.ClearColor.g, state.ClearColor.b, state.ClearColor.a}});
		preview.SetSizeLimits({0, 40}, {1000000, 40});
		const char* channels[] = {"Red", "Green", "Blue", "Alpha"};
		for (int channel = 0; channel < 4; ++channel)
			Number(color, channels[channel], state.ClearColor[channel], [this, channel](float value) {
				mHistory.EditScene("Scene clear color", [&]() {
					auto current = EditorSceneState::Capture();
					current.ClearColor[channel] = value;
					current.Restore();
				});
				mChanged();
			}, 0, 1);
		SetContent(std::move(root));
	}
}
