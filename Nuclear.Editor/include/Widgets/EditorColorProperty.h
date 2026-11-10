#pragma once
#include <UI/Button.h>
#include <Math/Math.h>
#include <functional>
#include <string>

namespace Nuclear::Editor
{
	class EditorColorPickerWindow;
	/// Compact color swatch button. The full picker exists only in a separate native window after activation.
	class EditorColorProperty : public UI::Button
	{
	public:
		/// Borrows the window service for this property's lifetime; copies title, linear RGB and value callback.
		EditorColorProperty(EditorColorPickerWindow& windows, const std::string& title, Math::Vector3 color,
			std::function<void(Math::Vector3)> committed, float maximum = 1);
		/// RGBA property with editable opacity. Apply commits once; Cancel leaves the model untouched.
		EditorColorProperty(EditorColorPickerWindow& windows, const std::string& title, Math::Vector4 color,
			std::function<void(Math::Vector4)> committed, float maximum = 1);
	private:
		/// Builds the button's swatch and label and connects a deferred native-window open request.
		void Build(EditorColorPickerWindow& windows, const std::string& title, Math::Vector4 color,
			bool alpha, float maximum, std::function<void(Math::Vector4)> committed);
		std::shared_ptr<void> mLifetime = std::make_shared<int>(0); ///< Weakly tracked by the dialog; expires before widget destruction.
	};
}
