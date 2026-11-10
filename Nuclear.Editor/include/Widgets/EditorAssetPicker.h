#pragma once
#include <UI/PanelWidget.h>
#include <Assets/IAsset.h>
#include <functional>
#include <Nuclear.Platform/Event.h>

namespace Nuclear::UI { class ScrollBox; class TextBox; }

namespace Nuclear::Editor
{
	/// Context-owned asset dropdown with a clear action, name search and a scrollable typed registry list.
	class EditorAssetPicker : public UI::PanelWidget
	{
	public:
		/// owner is the attached property ID; callbacks are used only while that property remains visible.
		/// Registry entries are resolved again by UUID before committing; assets remain library-owned.
		EditorAssetPicker(UI::WidgetId owner, Assets::AssetType type,
			std::function<void(Assets::IAsset*)> committed);
		void FocusSearch(); ///< Focuses the search input after attachment and deferred context insertion.
		/// Called by the editor's context shortcut filter. Dismisses on native focus loss;
		/// returns true when Escape was consumed. Other events continue through retained input.
		static bool ProcessEvent(UI::UIContext& context, const Platform::Event& event);
		/// Called outside input dispatch for each live context. Closes a chooser whose property
		/// became hidden/disabled or whose keyboard focus moved outside the dropdown.
		static void Update(UI::UIContext& context);
		UI::EventReply OnInput(const UI::InputEvent& event) override; ///< Dismisses on outside press, Escape or focus loss.
	protected:
		bool AcceptsPointer() const override; ///< Blocks pointer input while the owner is visible and enabled.
		void OnAttached() override; ///< Replaces any other asset picker in the same native context.
		void OnRemoved() override; ///< Drops this context's picker registration before teardown.
		void OnArrange(UI::Rect content) override; ///< Fits the dropdown below or above its owner in logical units.
		void OnPaint(UI::DrawList& list, UI::Rect clip) const override; ///< Paints an opaque dropdown frame.
	private:
		void Filter(const std::string& text); ///< Rebuilds only the results; preserves search focus and caret.
		void Commit(Assets::IAsset* asset); ///< Closes first, then invokes the callback for a live visible owner.
		void Close(); ///< Defers root removal safely during input dispatch.
		UI::WidgetId mOwner; ///< Property identity in the same context; never a borrowed widget pointer.
		Assets::AssetType mType; ///< Mesh or Material registry category.
		std::function<void(Assets::IAsset*)> mCommitted; ///< Copied property commit callback.
		UI::TextBox* pSearch = nullptr; ///< Owned search input.
		UI::ScrollBox* pResults = nullptr; ///< Owned clipped result list.
		UI::Rect mPopup; ///< Arranged dropdown bounds for outside-click dismissal.
	};
}
