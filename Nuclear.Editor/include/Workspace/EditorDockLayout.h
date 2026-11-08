#pragma once
#include <EditorDockArea.h>
#include <array>

namespace Nuclear::UI { class Border; }

namespace Nuclear::Editor
{
	/// Default workspace regions; tabs can move between these groups or into native windows.
	enum class EditorDockRegion { Left, Documents, Right, BottomLeft, BottomRight };

	/// Retained dock surface with collapsible empty groups, draggable splitters and drop previews.
	class EditorDockLayout : public UI::PanelWidget
	{
	public:
		EditorDockLayout(); ///< Builds five persistent, initially empty tab groups.
		EditorDockArea& GetArea(EditorDockRegion region); ///< Borrowed group for the supplied region.
		/// Returns the region under a logical drop position, including edge targets for empty groups.
		EditorDockRegion DropRegion(UI::Vec2 point) const;
		/// Shows a translucent docking target; pass -1 to clear it.
		void SetDropPreview(int region);
		void ResetSplits(); ///< Restores the initial layout proportions.
		bool IsFocusable() const override { return true; } ///< Receives Escape during a captured splitter drag.
		UI::EventReply OnInput(const UI::InputEvent& event) override; ///< Captures and resizes splitters.
	protected:
		UI::Vec2 OnMeasure(UI::Vec2 available) override; ///< Measures all groups without rebuilding them.
		void OnArrange(UI::Rect content) override; ///< Arranges only nonempty tool groups.
		void OnPaint(UI::DrawList& list, UI::Rect clip) const override; ///< Paints background and drop preview.
		bool AcceptsPointer() const override { return true; } ///< Empty space receives splitter input.
	private:
		UI::Rect RegionBounds(EditorDockRegion region, bool includeEmpty) const; ///< Logical region allocation.
		std::array<EditorDockArea*, 5> mAreas{}; ///< Child-owned persistent dock groups.
		UI::Border* pPreview = nullptr; ///< Child-owned translucent drop overlay, painted after the groups.
		float mLeft = .21f; ///< Left-column fraction of available width.
		float mRight = .27f; ///< Right-column fraction of available width.
		float mBottom = .34f; ///< Bottom-row fraction, with a 180-unit minimum capped at 60% of the available height.
		float mBottomSplit = .59f; ///< Bottom-left fraction of width remaining beside the right column.
		std::array<float, 4> mOriginal{}; ///< Split proportions saved for Escape/cancellation.
		int mSplitter = -1; ///< Captured splitter index, or -1 when idle.
		int mPreview = -1; ///< Highlighted docking region index, or -1.
	};
}
