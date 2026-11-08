#include <EditorSettings.h>
#include <Core/Engine.h>
#include <algorithm>

namespace Nuclear::Editor
{
	EditorSettings& EditorSettings::Get()
	{
		static EditorSettings instance;
		return instance;
	}

	EditorSettings::EditorSettings()
		: mAccentColor(GetDefaultAccentColor())
	{
	}

	UI::Color EditorSettings::GetDefaultAccentColor()
	{
		return {.18f, .70f, .62f, 1.0f};
	}

	UI::Color EditorSettings::GetAccentColor() const
	{
		return mAccentColor;
	}

	void EditorSettings::SetAccentColor(const UI::Color& color)
	{
		mAccentColor = {
			std::clamp(color.r, 0.0f, 1.0f),
			std::clamp(color.g, 0.0f, 1.0f),
			std::clamp(color.b, 0.0f, 1.0f),
			1.0f
		};
		if (OnAccentColorChanged)
			OnAccentColorChanged(mAccentColor);
	}

	void EditorSettings::ResetAccentColor()
	{
		SetAccentColor(GetDefaultAccentColor());
	}

	bool EditorSettings::GetVSync() const
	{
		return mVSync;
	}

	void EditorSettings::SetVSync(bool enabled)
	{
		mVSync = enabled;
		Core::Engine::Get().SetSwapChainSyncInterval(enabled ? 1 : 0);
		if (OnVSyncChanged)
			OnVSyncChanged(enabled);
	}
} // namespace Nuclear::Editor
