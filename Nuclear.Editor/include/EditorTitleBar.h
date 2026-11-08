#pragma once
#include <EditorMenuPopup.h>
#include <Nuclear.Platform/Window.h>
#include <UI/TextBlock.h>

namespace Nuclear::Editor
{
	/// Shared retained title bar for main and detached editor windows. Window outlives the widget.
	class EditorTitleBar : public UI::PanelWidget
	{
	public:
		explicit EditorTitleBar(Platform::Window& window); ///< Builds logo and native caption controls.
		void AddMenu(const std::string& title, EditorMenuPopup::Items items); ///< Adds a dropdown before the caption.
		/// Supplies current commands when this named menu opens, including current enabled states.
		void SetMenuProvider(const std::string& title, std::function<EditorMenuPopup::Items()> provider);
		void SetTitle(const std::string& title); ///< Updates the retained caption and native taskbar/Alt-Tab title.
		void CloseMenu(); ///< Removes any open popup through the owning context.
		void UpdateMenu(); ///< Closes the popup when a command loses focus or its owner becomes hidden/disabled; call each frame.
	protected:
		UI::Vec2 OnMeasure(UI::Vec2 available) override; ///< Fixed 40-unit caption height.
		void OnArrange(UI::Rect content) override; ///< Logo/menus left, controls flush right, caption in remaining space.
		void OnPaint(UI::DrawList& list, UI::Rect clip) const override; ///< Dark caption background.
		void OnAttached() override; ///< Installs the window's physical-pixel drag-region callback.
		void OnRemoved() override; ///< Drops popups and callback before this widget expires.
		bool AcceptsPointer() const override { return true; } ///< Empty caption space is a native drag target.
	private:
		/// One dropdown's metadata; the panel owns its button.
		struct Menu
		{
			UI::Widget* Button = nullptr; ///< Borrowed child with stable lifetime.
			EditorMenuPopup::Items Items; ///< Copied commands, evaluated on activation.
			std::string Title; ///< Stable menu name for provider registration.
			std::function<EditorMenuPopup::Items()> Provider; ///< Optional dynamic command supplier.
		};
		void OpenMenu(size_t index); ///< Replaces the current dropdown; callbacks remove it before execution.
		void NavigateMenu(UI::Vec2 point, bool clicked); ///< Switches on header hover; outside click dismisses.
		Platform::Window& mWindow; ///< Borrowed native owner.
		std::vector<Menu> mMenus; ///< Header buttons and command descriptions.
		UI::Widget* pLogo = nullptr; ///< Borrowed image child.
		UI::TextBlock* pCaption = nullptr; ///< Borrowed caption child.
		UI::Widget* mControls[3]{}; ///< Borrowed minimize/maximize/close children.
		UI::WidgetId mPopup = 0; ///< Context-owned menu overlay identity.
		size_t mOpenMenu = size_t(-1); ///< Open header index, or invalid when collapsed.
	};
}
