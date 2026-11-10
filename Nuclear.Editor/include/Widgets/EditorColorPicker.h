#pragma once
#include <Math/Math.h>
#include <UI/ContentWidget.h>
#include <array>
#include <functional>
#include <string>

namespace Nuclear::UI { class TextBox; }

namespace Nuclear::Editor
{
	class EditorColorWheel;
	/// Shared editor color property with a wheel, RGB(A), HSV, and HEX editors kept in sync.
	/// RGB/HSV use linear values; HEX encodes clamped linear channels as #RRGGBB or #RRGGBBAA.
	/// Text commits on Enter, gestures on release. Invalid input restores the last valid value.
	class EditorColorPicker : public UI::ContentWidget
	{
	public:
		/// Copies a vec3 color and callback; maximum preserves HDR light colors when greater than one.
		EditorColorPicker(Math::Vector3 color, std::function<void(Math::Vector3)> committed, float maximum = 1);
		/// Copies a vec4 color and callback; alpha is always constrained to [0,1].
		EditorColorPicker(Math::Vector4 color, std::function<void(Math::Vector4)> committed, float maximum = 1);
		/// Returns the current preview/committed linear RGBA (alpha 1 for vec3).
		Math::Vector4 GetColor() const;
		/// Replaces the color and synchronizes editors without invoking the callback. Cancels any active gesture.
		void SetColor(Math::Vector3 color);
		/// Replaces linear RGBA without invoking the callback; vec3 pickers ignore alpha.
		void SetColor(Math::Vector4 color);
	private:
		/// Builds the owning child tree once; borrowed control pointers live as long as this picker.
		void Build(Math::Vector4 color);
		/// Refreshes numeric/HEX strings after programmatic changes or wheel previews.
		void Synchronize();
		/// Validates a finite in-range scalar, applies its channel and commits once.
		void CommitNumber(int channel, const std::string& text);
		/// Parses exactly six/eight hexadecimal digits (optional #), then commits once.
		void CommitHex(const std::string& text);
		/// Synchronizes before notifying the owner; unchanged colors produce no callback.
		void Commit();
		EditorColorWheel* pWheel = nullptr; ///< Borrowed child owned by this picker.
		std::array<UI::TextBox*, 7> mInputs = {}; ///< Borrowed RGB, alpha, H, S and V fields (alpha may be null).
		UI::TextBox* pHex = nullptr; ///< Borrowed HEX field.
		Math::Vector4 mCommitted = {1, 1, 1, 1}; ///< Last committed color for no-op suppression.
		std::function<void(Math::Vector4)> mCallback; ///< Value callback; may rebuild/remove the picker.
		float mMaximum = 1; ///< Inclusive linear RGB/value limit.
		bool mEditAlpha = false; ///< Constructor-selected vec3/vec4 mode.
	};
}
