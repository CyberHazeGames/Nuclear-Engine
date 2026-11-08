#pragma once
#include <UI/PanelWidget.h>
#include <array>
#include <functional>

namespace Nuclear::UI { class Button; }
namespace Nuclear::Editor
{
	/// Full-viewport Add popup with a cascading Shapes submenu; the context owns its lifetime.
	class EditorAddMenu : public UI::PanelWidget
	{
	public:
		/// Anchor is the toolbar button in logical units. Create receives 0=empty, 1=box, 2=sphere,
		/// 3=plane or 4=skybox. Dismiss runs before creation; captured controllers outlive this popup.
		EditorAddMenu(UI::Rect anchor, std::function<void(int)> create, std::function<void()> dismiss);
		bool IsFocusable() const override { return true; } ///< Receives keyboard navigation on opening.
		UI::EventReply OnInput(const UI::InputEvent& event) override; ///< Arrows/Enter navigate, Escape/outside/focus loss dismiss.
	protected:
		bool AcceptsPointer() const override { return true; } ///< Blocks input to the workspace beneath the popup.
		void OnArrange(UI::Rect content) override; ///< Positions both columns within the logical viewport.
		void OnPaint(UI::DrawList& list, UI::Rect clip) const override; ///< Paints menu and submenu backgrounds.
	private:
		void SetShapesOpen(bool open); ///< Shows or collapses submenu controls and invalidates layout.
		void Highlight(); ///< Synchronizes keyboard selection colors without rebuilding controls.
		void Execute(size_t index); ///< Dismisses then creates the selected entity, or opens Shapes.
		UI::Rect mAnchor; ///< Borrowed button's geometry at opening.
		UI::Rect mMenu; ///< Arranged root menu rectangle.
		UI::Rect mShapes; ///< Arranged submenu rectangle.
		std::array<UI::Button*, 6> mButtons{}; ///< Owned children: Empty, Shapes, Skybox, Box, Plane, Sphere.
		std::function<void(int)> mCreate; ///< Queues or applies one selected creation action.
		std::function<void()> mDismiss; ///< Safe context-deferred removal callback.
		size_t mSelected = 0; ///< Current root or submenu button index.
		bool mShapesOpen = false; ///< Submenu visibility and keyboard scope.
	};
}
