#pragma once
#include <UI/PanelWidget.h>
#include <UI/ContentWidget.h>

namespace Nuclear::Editor
{
	/// Asset document surface with a full-height preview on the left and scrollable properties on the right.
	class AssetViewerLayout : public UI::PanelWidget
	{
	public:
		AssetViewerLayout(); ///< Creates two owned content hosts and a draggable vertical divider.
		UI::ContentWidget& GetPreview() { return *pPreview; } ///< Owned preview host, valid for this layout's lifetime.
		UI::ContentWidget& GetProperties() { return *pProperties; } ///< Owned right-side properties host.
		bool IsFocusable() const override { return true; } ///< Receives Escape to cancel a divider drag.
		UI::EventReply OnInput(const UI::InputEvent& event) override; ///< Captures divider drags; Escape/focus loss restores its starting ratio.
	protected:
		bool AcceptsPointer() const override { return true; } ///< Receives input in the divider's six logical units.
		void OnArrange(UI::Rect content) override; ///< Preserves usable preview/properties proportions at the owning context's DPI.
		void OnPaint(UI::DrawList& list, UI::Rect clip) const override; ///< Paints the background and divider.
	private:
		UI::ContentWidget* pPreview = nullptr; ///< Child-owned left host.
		UI::ContentWidget* pProperties = nullptr; ///< Child-owned right host.
		UI::Rect mDivider; ///< Divider hit region in logical viewport units.
		float mPropertiesRatio = .28f; ///< Fraction of the document width reserved for properties.
		float mOriginalRatio = .28f; ///< Ratio saved before a captured drag for cancellation.
		bool mDragging = false; ///< True while this layout owns a divider gesture.
	};
}
