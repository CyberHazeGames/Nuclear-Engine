#pragma once
#include <Math/Math.h>
#include <UI/Widget.h>
#include <functional>

namespace Nuclear::Editor
{
	/// Retained hue/saturation disk with value and optional alpha strips and a checkerboard swatch.
	/// Gestures preview locally; release commits once, while Escape or input cancellation restores the start.
	class EditorColorWheel : public UI::Widget
	{
	public:
		/// RGB is linear and nonnegative, alpha is [0,1]; maximum bounds RGB/value (use >1 for HDR).
		explicit EditorColorWheel(bool alpha = false, float maximum = 1);
		/// Sets a color without callbacks. Achromatic colors preserve the last meaningful hue.
		void SetColor(Math::Vector4 color);
		/// Sets HSV in degrees, saturation [0,1], value [0,maximum], preserving alpha; no callbacks.
		void SetHSV(Math::Vector3 hsv);
		/// Returns linear RGBA; RGB-only wheels always return alpha 1.
		Math::Vector4 GetColor() const;
		/// Returns hue in [0,360), saturation [0,1], and linear value in [0,maximum].
		Math::Vector3 GetHSV() const;
		/// Called during a gesture and on cancellation, to synchronize sibling controls; must keep this widget alive.
		std::function<void()> OnPreview;
		/// Called once per changed release or keyboard adjustment. May remove this widget.
		std::function<void()> OnCommitted;
		/// Allows Tab focus and arrow-key hue/saturation changes.
		bool IsFocusable() const override;
		/// Handles captured wheel/strip drags, Escape rollback and arrow-key adjustments.
		UI::EventReply OnInput(const UI::InputEvent& event) override;
	protected:
		/// Reserves a compact surface in logical units; narrow layouts shrink the disk.
		UI::Vec2 OnMeasure(UI::Vec2 available) override;
		/// Paints the disk, strips, selection markers, and transparency preview with retained quads.
		void OnPaint(UI::DrawList& list, UI::Rect clip) const override;
	private:
		/// Returns disk (0), value (1), alpha (2), or preview (3) geometry in logical units.
		UI::Rect Region(int index) const;
		/// Updates the active drag region, clamping captured positions outside its bounds.
		void Drag(UI::Vec2 position);
		Math::Vector3 mHSV = {0, 0, 1}; ///< Current hue (degrees), saturation and value.
		Math::Vector3 mStartHSV = {0, 0, 1}; ///< Rollback snapshot for the current gesture.
		Math::Vector3 mRGB = {1, 1, 1}; ///< Exact linear RGB, avoiding round-trip drift on individual channel edits.
		Math::Vector3 mStartRGB = {1, 1, 1}; ///< Exact RGB at pointer-down, restored on cancellation.
		float mAlpha = 1; ///< Current unassociated opacity.
		float mStartAlpha = 1; ///< Opacity at pointer-down.
		float mMaximum = 1; ///< Inclusive RGB/value limit, at least 1.
		float mDragValueMaximum = 1; ///< Stable value-strip range throughout a gesture.
		bool mEditAlpha = false; ///< Whether an alpha strip is visible and editable.
		int mDragging = -1; ///< Active region, or -1 when no pointer gesture is active.
	};
}
