#pragma once
#include <UI/Widget.h>
#include <UI/DrawList.h>
#include <ECS/entt/entity/entity.hpp>
#include <TransformGizmo.h>
#include <array>
#include <functional>
namespace Nuclear::Components
{
	class EntityInfoComponent;
}
namespace Nuclear::Editor
{
	/// Retained scene surface with focused fly-camera navigation and optional world-axis transform handles.
	class GameViewPort : public UI::Widget
	{
	  public:
		/// Changes the borrowed ECS handle, cancelling and restoring any pending edit on the old target.
		void SetSelectedEntity(entt::entity entity);
		/// Cancels pending edits before changing tool; Select hides handles while retaining selection.
		/// Q/G/E/R select tools while this surface has keyboard focus.
		void SetGizmoMode(GizmoMode mode);
		GizmoMode GetGizmoMode() const
		{
			return mGizmo.GetMode();
		} ///< Current viewport tool, including Select when transform handles are disabled.
		/// Advances held WASD camera movement by deltaTime seconds at the camera's configured speed.
		/// Requires viewport keyboard focus; pauses during transform drags and clears keys on focus loss.
		/// Nonpositive or nonfinite time steps are ignored. Diagonal movement keeps the same speed.
		void UpdateCamera(float deltaTime);
		/// Called after drag updates/commit/cancel so the inspector can rebuild outside dispatch.
		std::function<void()> OnTransformChanged;
		bool IsFocusable() const override
		{
			return true;
		} ///< Receives focus and pointer capture.
		UI::EventReply OnInput(
			const UI::InputEvent& event) override; ///< Left edits handles; right looks; focused WASD moves.
	  protected:
		void OnArrange(UI::Rect content) override; ///< Cancels captured edits when viewport geometry changes.
		void OnPaint(UI::DrawList& list,
					 UI::Rect clip) const override; ///< Samples the current camera render target.
	  private:
		Components::EntityInfoComponent* SelectedInfo()
			const;				///< Resolves the live selected component, or null.
		void CancelTransform(); ///< Restores a live drag target and notifies the inspector.
		TransformGizmo mGizmo;	///< Handle drawing, hit testing and transform drag state.
		entt::entity mSelected =
			entt::null;			///< Validated on every input and paint; never a component pointer.
		UI::Rect mDragBounds;	///< Viewport allocation at interaction start, in logical units.
		bool mDragging = false; ///< A right-button camera-look gesture is active.
		UI::Vec2 mLastPointer;	///< Previous logical pointer position.
		std::array<bool, 4> mMovementKeys{}; ///< Held W/A/S/D keys received by this focused viewport.
	};
} // namespace Nuclear::Editor
