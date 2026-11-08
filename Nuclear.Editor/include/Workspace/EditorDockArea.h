#pragma once
#include <EditorTabStrip.h>
#include <UI/PanelWidget.h>

namespace Nuclear::Editor
{
	/// Owning tab group. Inactive contents remain attached but collapsed, preserving their state.
	class EditorDockArea : public UI::PanelWidget
	{
	public:
		EditorDockArea(); ///< Creates the local tab header.
		/// Adds detached contents at an insertion position; activates the new tab and owns its widget tree.
		void AddTab(EditorTabStrip::Tab tab, std::unique_ptr<UI::Widget> content, size_t index = SIZE_MAX);
		/// Transfers the contents of a stable tab ID outside dispatch; null if it is absent.
		std::unique_ptr<UI::Widget> TakeTab(const std::string& id);
		/// Activates one tab without rebuilding contents, cancelling input in the previous tab.
		void Activate(const std::string& id);
		/// Changes one tab caption while retaining its stable identity and current selection.
		void SetTabTitle(const std::string& id, const std::string& title);
		/// Controls the local header; the main document group uses the top-level browser strip instead.
		void ShowHeader(bool show);
		const std::vector<EditorTabStrip::Tab>& GetTabs() const { return mTabs; } ///< Borrowed display order.
		const std::string& GetActive() const { return mActive; } ///< Selected stable identity, or empty.
		EditorTabStrip& GetStrip() { return *pStrip; } ///< Borrowed local header.
		std::function<void()> OnChanged; ///< Notifies the workspace to refresh external headers/layout.
	protected:
		UI::Vec2 OnMeasure(UI::Vec2 available) override; ///< Measures contents within a dock allocation.
		void OnArrange(UI::Rect content) override; ///< Allocates header and the selected content.
		void OnPaint(UI::DrawList& list, UI::Rect clip) const override; ///< Panel background.
	private:
		std::vector<EditorTabStrip::Tab> mTabs; ///< Metadata in browser order.
		std::vector<UI::WidgetId> mContents; ///< Child identities corresponding to mTabs.
		EditorTabStrip* pStrip = nullptr; ///< Child-owned local header.
		std::string mActive; ///< Selected stable identity.
		bool mShowHeader = true; ///< False when an external browser strip controls this group.
	};
}
