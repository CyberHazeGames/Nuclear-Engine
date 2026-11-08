#pragma once
#include <UI/Types.h>
#include <functional>
#include <string>

namespace Nuclear::Editor
{
	/// Central repository and coordinator for editor preferences and appearance settings.
	/// Manages the active accent color, theme presets, and notifies listeners when settings are modified.
	class EditorSettings
	{
	public:
		/// Returns the shared global singleton instance of editor settings.
		static EditorSettings& Get();

		/// Returns the current editor accent color used for highlights, active tab lines, and section headings.
		UI::Color GetAccentColor() const;

		/// Sets the editor accent color and notifies registered listeners.
		///
		/// @param color The linear RGBA color to use as the editor accent. Values are clamped to [0, 1], with alpha forced to 1.
		void SetAccentColor(const UI::Color& color);

		/// Resets the accent color back to the engine's default cyan/teal tone.
		void ResetAccentColor();

		/// Callback invoked whenever the accent color changes, enabling editor panels to refresh.
		std::function<void(const UI::Color&)> OnAccentColorChanged;

		/// Returns whether vertical synchronization (VSync) is enabled for swap chain presentation.
		bool GetVSync() const;

		/// Sets vertical synchronization (VSync) enabled or disabled and updates the swap chain sync interval.
		///
		/// @param enabled True to synchronize presentation with the display refresh rate (interval 1), false for immediate (interval 0).
		void SetVSync(bool enabled);

		/// Callback invoked whenever the VSync setting is toggled.
		std::function<void(bool)> OnVSyncChanged;

		/// Returns the default accent color for Nuclear Engine editor ({0.18f, 0.70f, 0.62f, 1.0f}).
		static UI::Color GetDefaultAccentColor();

	private:
		EditorSettings();
		~EditorSettings() = default;

		UI::Color mAccentColor; ///< Active accent color.
		bool mVSync = true;		///< Vertical synchronization state.
	};
} // namespace Nuclear::Editor
