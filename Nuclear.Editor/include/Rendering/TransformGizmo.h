#pragma once

#include <ECS/Transform.h>
#include <UI/DrawList.h>

namespace Nuclear::Components
{
	class CameraComponent;
}

namespace Nuclear::Editor
{
	/// Viewport tool; Select keeps the entity selected without showing transform handles.
	enum class GizmoMode
	{
		Move,
		Rotate,
		Scale,
		Select ///< Disables handle drawing, hit testing and transform editing.
	};

	/// Screen-sized transform handles drawn with retained UI primitives. Owns drag state,
	/// never an entity or transform pointer; the viewport resolves ECS handles on each event.
	class TransformGizmo
	{
	  public:
		/// Selects the operation between interactions. Cancel an active drag before changing it.
		void SetMode(GizmoMode mode);
		GizmoMode GetMode() const
		{
			return mMode;
		} ///< Current operation.
		bool IsDragging() const
		{
			return mAxis >= 0;
		} ///< True while a handle owns a pending edit.
		/// Returns logical handle coordinates, false in Select mode or for invalid axes/invisible pivots.
		/// Axes 0..2 are world XYZ; 3 is the Move/Scale center. Rotation angle is in radians.
		bool GetHandlePosition(const ECS::Transform& transform, const Components::CameraComponent& camera,
							   UI::Rect viewport, int axis, UI::Vec2& position, float angle = .7f) const;
		/// Updates hover highlighting without changing a transform.
		void UpdateHover(const ECS::Transform& transform, const Components::CameraComponent& camera,
						 UI::Rect viewport, UI::Vec2 pointer);
		/// Starts a drag on a visible handle and snapshots the complete starting transform.
		bool BeginDrag(const ECS::Transform& transform, const Components::CameraComponent& camera,
					   UI::Rect viewport, UI::Vec2 pointer);
		/// Applies a captured pointer sample. Returns true only when a finite edit was applied.
		bool UpdateDrag(ECS::Transform& transform, const Components::CameraComponent& camera,
						UI::Rect viewport, UI::Vec2 pointer);
		/// Commits the edit already applied to the transform and releases internal drag state.
		void EndDrag();
		/// Restores the snapshot when the still-valid transform is supplied; null discards a deleted target.
		/// Returns true when a live transform was restored.
		bool CancelDrag(ECS::Transform* transform);
		/// Draws clipped handles over the scene, or nothing in Select mode; borrows the font for this call.
		/// showHover suppresses an idle highlight when the pointer leaves the viewport.
		void Paint(UI::DrawList& list, UI::Rect clip, UI::Rect viewport, const ECS::Transform& transform,
				   const Components::CameraComponent& camera, const Assets::Font* font,
				   bool showHover = true) const;

	  private:
		/// Projects world coordinates through the camera's actual View field (GetViewMatrix returns Model).
		static bool Project(Math::Vector3 world, const Components::CameraComponent& camera, UI::Rect viewport,
							UI::Vec2& screen);
		/// Builds a world-space ray from logical coordinates using zero-to-one clip depth.
		static bool Ray(UI::Vec2 screen, const Components::CameraComponent& camera, UI::Rect viewport,
						Math::Vector3& origin, Math::Vector3& direction);
		/// Computes a world length whose vertical projection is approximately 76 logical units.
		static float HandleLength(Math::Vector3 pivot, const Components::CameraComponent& camera,
								  UI::Rect viewport);
		/// Finds the closest axis/ring within a seven-unit hit tolerance; -1 means no handle.
		int HitTest(const ECS::Transform& transform, const Components::CameraComponent& camera,
					UI::Rect viewport, UI::Vec2 pointer, float* ringAngle = nullptr) const;
		GizmoMode mMode = GizmoMode::Move;				 ///< Active operation.
		int mAxis = -1;									 ///< Captured XYZ axis or center index; -1 when idle.
		int mHoveredAxis = -1;							 ///< Highlighted idle handle.
		Math::Vector3 mStartPosition = Math::Vector3(0); ///< Snapshot in world units.
		Math::Vector3 mStartScale = Math::Vector3(1);	 ///< Snapshot of dimensionless component scales.
		Math::Quaternion mStartRotation =
			Math::Quaternion(1, 0, 0, 0);					  ///< Original quaternion, preserved on cancel.
		Math::Vector3 mPlaneNormal = Math::Vector3(0);		  ///< Free-move or rotation drag plane normal.
		Math::Vector3 mStartHit = Math::Vector3(0);			  ///< Initial plane intersection.
		Math::Vector3 mLastRotationVector = Math::Vector3(0); ///< Previous normalized radial vector.
		UI::Vec2 mStartPointer;								  ///< Initial logical pointer position.
		UI::Vec2 mLastPointer;								  ///< Previous logical pointer position.
		UI::Vec2 mRotationTangent;	   ///< Screen tangent used when a rotation plane is viewed edge-on.
		float mLength = 1;			   ///< Fixed world handle length at drag start.
		float mStartAxisParameter = 0; ///< Initial closest point along the world axis, in world units.
		float mAngle = 0;			   ///< Accumulated signed rotation in radians, including complete turns.
		bool mUseRotationPlane = true; ///< False selects stable tangent dragging for edge-on rings.
	};
} // namespace Nuclear::Editor
