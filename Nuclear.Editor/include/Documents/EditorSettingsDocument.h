#pragma once
#include <UI/Widget.h>
#include <memory>
#include <string>

namespace Nuclear::Editor
{
	class EditorColorPickerWindow;
	/// Builds the Editor Settings document window.
	/// Features a left vertical navigation menu listing settings categories (Theme, General, Viewport)
	/// and a right main view displaying the selected settings page with interactive properties.
	class EditorSettingsDocument
	{
	public:
		/// Returns the document tab title ("Editor Settings").
		static std::string GetTitle();

		/// Constructs the two-pane Editor Settings view with navigation sidebar and settings page content.
		///
		/// @return Owning pointer to the root UI::Widget containing the full settings layout.
		/// colors is borrowed by color properties and must outlive the document.
		static std::unique_ptr<UI::Widget> Build(EditorColorPickerWindow& colors);
	};

	/// Convenience alias matching window-centric naming conventions.
	using EditorSettingsWindow = EditorSettingsDocument;
} // namespace Nuclear::Editor
