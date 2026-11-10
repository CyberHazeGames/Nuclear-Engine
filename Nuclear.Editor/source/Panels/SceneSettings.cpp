#include <SceneSettings.h>
#include <EditorHistory.h>
#include <EditorWidgets.h>
#include <EditorColorProperty.h>

namespace Nuclear::Editor
{
	SceneSettings::SceneSettings(EditorHistory& history, std::function<void()> changed, EditorColorPickerWindow& colors)
		: mHistory(history), mColors(colors), mChanged(std::move(changed))
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
		color.Add<EditorColorProperty>(mColors, "Clear color", state.ClearColor, [this](Math::Vector4 value) {
				mHistory.EditScene("Scene clear color", [&]() {
					auto current = EditorSceneState::Capture();
					current.ClearColor = value;
					current.Restore();
				});
				mChanged();
			});
		SetContent(std::move(root));
	}
}
