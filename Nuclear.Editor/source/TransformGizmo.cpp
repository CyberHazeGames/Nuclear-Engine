#include <TransformGizmo.h>
#include <Components/CameraComponent.h>
#include <cmath>

namespace Nuclear::Editor
{
	namespace
	{
		constexpr float Pi = 3.14159265358979323846f;
		constexpr int RingSegments = 96;

		Math::Vector3 Axis(int axis)
		{
			Math::Vector3 direction(0);
			direction[axis] = 1;
			return direction;
		}

		Math::Vector3 RingVector(int axis, float angle)
		{
			return Axis((axis + 1) % 3) * std::cos(angle) + Axis((axis + 2) % 3) * std::sin(angle);
		}

		bool Finite(Math::Vector3 value)
		{
			return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
		}

		float Distance(UI::Vec2 left, UI::Vec2 right)
		{
			return std::hypot(left.x - right.x, left.y - right.y);
		}

		float SegmentDistance(UI::Vec2 point, UI::Vec2 start, UI::Vec2 end, float* fraction = nullptr)
		{
			const float dx = end.x - start.x;
			const float dy = end.y - start.y;
			const float squared = dx * dx + dy * dy;
			const float t =
				squared > .0001f
					? std::clamp(((point.x - start.x) * dx + (point.y - start.y) * dy) / squared, 0.f, 1.f)
					: 0;
			if (fraction)
				*fraction = t;
			return Distance(point, {start.x + dx * t, start.y + dy * t});
		}

		void Line(UI::DrawList& list, UI::Rect clip, UI::Vec2 start, UI::Vec2 end, UI::Color color,
				  float width)
		{
			const int steps = std::clamp(static_cast<int>(std::ceil(Distance(start, end) / 1.5f)), 1, 512);
			for (int index = 0; index <= steps; ++index)
			{
				const float t = static_cast<float>(index) / steps;
				list.Add({start.x + (end.x - start.x) * t - width / 2,
						  start.y + (end.y - start.y) * t - width / 2, width, width},
						 clip, color);
			}
		}

		bool PlaneHit(Math::Vector3 origin, Math::Vector3 direction, Math::Vector3 pivot,
					  Math::Vector3 normal, Math::Vector3& hit)
		{
			const float denominator = Math::dot(direction, normal);
			if (std::abs(denominator) < .0001f)
				return false;
			const float distance = Math::dot(pivot - origin, normal) / denominator;
			if (!std::isfinite(distance) || distance < 0)
				return false;
			hit = origin + direction * distance;
			return Finite(hit);
		}

		bool AxisParameter(Math::Vector3 origin, Math::Vector3 direction, Math::Vector3 pivot,
						   Math::Vector3 axis, float& parameter)
		{
			const float parallel = Math::dot(direction, axis);
			const float denominator = 1 - parallel * parallel;
			if (denominator < .001f)
				return false;
			const auto offset = origin - pivot;
			parameter = (Math::dot(offset, axis) - parallel * Math::dot(offset, direction)) / denominator;
			return std::isfinite(parameter);
		}

		UI::Color AxisColor(int axis, bool highlighted)
		{
			if (highlighted)
				return {1, .85f, .18f, 1};
			const UI::Color colors[] = {{1, .2f, .16f, 1}, {.18f, .95f, .32f, 1}, {.2f, .5f, 1, 1}};
			return colors[axis];
		}
	} // namespace

	void TransformGizmo::SetMode(GizmoMode mode)
	{
		mMode = mode;
		mHoveredAxis = -1;
	}

	bool TransformGizmo::Project(Math::Vector3 world, const Components::CameraComponent& camera,
								 UI::Rect viewport, UI::Vec2& screen)
	{
		const auto clip = camera.GetProjectionMatrix() * camera.mCameraData.View * Math::Vector4(world, 1);
		if (!Finite(Math::Vector3(clip)) || !std::isfinite(clip.w) || clip.w <= .0001f || clip.z < 0 ||
			clip.z > clip.w)
			return false;
		screen = {viewport.x + (clip.x / clip.w + 1) * viewport.Width * .5f,
				  viewport.y + (1 - clip.y / clip.w) * viewport.Height * .5f};
		return std::isfinite(screen.x) && std::isfinite(screen.y);
	}

	bool TransformGizmo::Ray(UI::Vec2 screen, const Components::CameraComponent& camera, UI::Rect viewport,
							 Math::Vector3& origin, Math::Vector3& direction)
	{
		if (viewport.Width < 1 || viewport.Height < 1)
			return false;
		const float x = (screen.x - viewport.x) / viewport.Width * 2 - 1;
		const float y = 1 - (screen.y - viewport.y) / viewport.Height * 2;
		const auto inverse = Math::inverse(camera.GetProjectionMatrix() * camera.mCameraData.View);
		auto nearPoint = inverse * Math::Vector4(x, y, 0, 1);
		auto farPoint = inverse * Math::Vector4(x, y, 1, 1);
		if (std::abs(nearPoint.w) < .000001f || std::abs(farPoint.w) < .000001f)
			return false;
		origin = Math::Vector3(nearPoint) / nearPoint.w;
		const auto delta = Math::Vector3(farPoint) / farPoint.w - origin;
		if (!Finite(origin) || !Finite(delta) || Math::dot(delta, delta) < .000001f)
			return false;
		direction = Math::normalize(delta);
		return true;
	}

	float TransformGizmo::HandleLength(Math::Vector3 pivot, const Components::CameraComponent& camera,
									   UI::Rect viewport)
	{
		UI::Vec2 screen;
		if (viewport.Width < 32 || viewport.Height < 32 || !Project(pivot, camera, viewport, screen) ||
			!viewport.Contains(screen))
			return 0;
		const auto clip = camera.GetProjectionMatrix() * camera.mCameraData.View * Math::Vector4(pivot, 1);
		const float projection = std::abs(camera.GetProjectionMatrix()[1][1]);
		if (projection < .0001f)
			return 0;
		return std::min(76.f * 2 * clip.w / (projection * viewport.Height), clip.w * .45f);
	}

	bool TransformGizmo::GetHandlePosition(const ECS::Transform& transform,
										   const Components::CameraComponent& camera, UI::Rect viewport,
										   int axis, UI::Vec2& position, float angle) const
	{
		if (mMode == GizmoMode::Select || axis < 0 || axis > 3 || (axis == 3 && mMode == GizmoMode::Rotate))
			return false;
		const auto pivot = transform.GetLocalPosition();
		const float length = HandleLength(pivot, camera, viewport);
		if (length <= 0)
			return false;
		const auto offset =
			axis == 3 ? Math::Vector3(0)
					  : (mMode == GizmoMode::Rotate ? RingVector(axis, angle) : Axis(axis)) * length;
		return Project(pivot + offset, camera, viewport, position);
	}

	int TransformGizmo::HitTest(const ECS::Transform& transform, const Components::CameraComponent& camera,
								UI::Rect viewport, UI::Vec2 pointer, float* ringAngle) const
	{
		if (mMode == GizmoMode::Select || !viewport.Contains(pointer))
			return -1;
		UI::Vec2 center;
		if (!Project(transform.GetLocalPosition(), camera, viewport, center) ||
			HandleLength(transform.GetLocalPosition(), camera, viewport) <= 0)
			return -1;
		if (mMode != GizmoMode::Rotate && Distance(pointer, center) <= 9)
			return 3;
		float closest = 7;
		int hit = -1;
		for (int axis = 0; axis < 3; ++axis)
		{
			if (mMode != GizmoMode::Rotate)
			{
				UI::Vec2 end;
				if (!GetHandlePosition(transform, camera, viewport, axis, end) || Distance(center, end) < 14)
					continue;
				const float fraction = 12 / Distance(center, end);
				UI::Vec2 start = {center.x + (end.x - center.x) * fraction,
								  center.y + (end.y - center.y) * fraction};
				const float distance = SegmentDistance(pointer, start, end);
				if (distance < closest)
				{
					closest = distance;
					hit = axis;
				}
			}
			else
			{
				for (int segment = 0; segment < RingSegments; ++segment)
				{
					const float angle = 2 * Pi * segment / RingSegments;
					UI::Vec2 start, end;
					if (!GetHandlePosition(transform, camera, viewport, axis, start, angle) ||
						!GetHandlePosition(transform, camera, viewport, axis, end,
										   angle + 2 * Pi / RingSegments))
						continue;
					float fraction;
					const float distance = SegmentDistance(pointer, start, end, &fraction);
					if (distance < closest)
					{
						closest = distance;
						hit = axis;
						if (ringAngle)
							*ringAngle = angle + fraction * 2 * Pi / RingSegments;
					}
				}
			}
		}
		return hit;
	}

	void TransformGizmo::UpdateHover(const ECS::Transform& transform,
									 const Components::CameraComponent& camera, UI::Rect viewport,
									 UI::Vec2 pointer)
	{
		if (!IsDragging())
			mHoveredAxis = HitTest(transform, camera, viewport, pointer);
	}

	bool TransformGizmo::BeginDrag(const ECS::Transform& transform, const Components::CameraComponent& camera,
								   UI::Rect viewport, UI::Vec2 pointer)
	{
		float ringAngle = 0;
		const int axis = HitTest(transform, camera, viewport, pointer, &ringAngle);
		if (axis < 0 || IsDragging())
			return false;
		Math::Vector3 origin, direction;
		if (!Ray(pointer, camera, viewport, origin, direction))
			return false;
		mStartPosition = transform.GetLocalPosition();
		mStartScale = transform.GetLocalScale();
		mStartRotation = transform.GetLocalRotation();
		mStartPointer = mLastPointer = pointer;
		mLength = HandleLength(mStartPosition, camera, viewport);
		mAngle = 0;
		if (mMode == GizmoMode::Rotate)
		{
			mPlaneNormal = Axis(axis);
			mUseRotationPlane = std::abs(Math::dot(direction, mPlaneNormal)) > .08f;
			if (mUseRotationPlane)
			{
				if (!PlaneHit(origin, direction, mStartPosition, mPlaneNormal, mStartHit))
					return false;
				const auto radial = mStartHit - mStartPosition;
				if (Math::dot(radial, radial) < .000001f)
					return false;
				mLastRotationVector = Math::normalize(radial);
			}
			else
			{
				UI::Vec2 before, after;
				if (!GetHandlePosition(transform, camera, viewport, axis, before, ringAngle - .05f) ||
					!GetHandlePosition(transform, camera, viewport, axis, after, ringAngle + .05f))
					return false;
				const float tangentLength = Distance(before, after);
				if (tangentLength < .1f)
					return false;
				mRotationTangent = {(after.x - before.x) / tangentLength,
									(after.y - before.y) / tangentLength};
			}
		}
		else if (axis == 3 && mMode == GizmoMode::Move)
		{
			mPlaneNormal = camera.GetFrontView();
			if (!PlaneHit(origin, direction, mStartPosition, mPlaneNormal, mStartHit))
				return false;
		}
		else if (axis < 3 &&
				 !AxisParameter(origin, direction, mStartPosition, Axis(axis), mStartAxisParameter))
			return false;
		mAxis = axis;
		return true;
	}

	bool TransformGizmo::UpdateDrag(ECS::Transform& transform, const Components::CameraComponent& camera,
									UI::Rect viewport, UI::Vec2 pointer)
	{
		if (!IsDragging() || !std::isfinite(pointer.x) || !std::isfinite(pointer.y))
			return false;
		Math::Vector3 origin, direction;
		if (!Ray(pointer, camera, viewport, origin, direction))
			return false;
		if (mMode == GizmoMode::Rotate)
		{
			float angle;
			if (mUseRotationPlane)
			{
				Math::Vector3 hit;
				if (!PlaneHit(origin, direction, mStartPosition, mPlaneNormal, hit))
					return false;
				const auto radial = hit - mStartPosition;
				if (Math::dot(radial, radial) < .000001f)
					return false;
				const auto current = Math::normalize(radial);
				angle = std::atan2(Math::dot(mPlaneNormal, Math::cross(mLastRotationVector, current)),
								   Math::dot(mLastRotationVector, current));
				mLastRotationVector = current;
			}
			else
				angle = ((pointer.x - mLastPointer.x) * mRotationTangent.x +
						 (pointer.y - mLastPointer.y) * mRotationTangent.y) /
						76.f;
			mAngle += angle;
			auto rotation = Math::dot(mStartRotation, mStartRotation) > .000001f
								? Math::normalize(mStartRotation)
								: Math::Quaternion(1, 0, 0, 0);
			transform.SetRotation(Math::normalize(Math::angleAxis(mAngle, Axis(mAxis)) * rotation));
		}
		else
		{
			float parameter = mStartAxisParameter;
			if (mAxis < 3 && !AxisParameter(origin, direction, mStartPosition, Axis(mAxis), parameter))
				return false;
			if (mMode == GizmoMode::Move)
			{
				Math::Vector3 position;
				if (mAxis == 3)
				{
					Math::Vector3 hit;
					if (!PlaneHit(origin, direction, mStartPosition, mPlaneNormal, hit))
						return false;
					position = mStartPosition + hit - mStartHit;
				}
				else
					position = mStartPosition + Axis(mAxis) * (parameter - mStartAxisParameter);
				if (!Finite(position))
					return false;
				transform.SetPosition(position);
			}
			else
			{
				const float amount = mAxis == 3
										 ? (pointer.x - mStartPointer.x - pointer.y + mStartPointer.y) / 100.f
										 : (parameter - mStartAxisParameter) / mLength;
				const float factor = std::exp(std::clamp(amount, -4.6f, 4.6f));
				auto scale = mStartScale;
				for (int axis = 0; axis < 3; ++axis)
				{
					if (mAxis != 3 && mAxis != axis)
						continue;
					const float sign = scale[axis] < 0 ? -1.f : 1.f;
					scale[axis] = sign * std::clamp(std::abs(scale[axis]) * factor, .0001f, 100000.f);
				}
				if (!Finite(scale))
					return false;
				transform.SetScale(scale);
			}
		}
		mLastPointer = pointer;
		transform.Update();
		return true;
	}

	void TransformGizmo::EndDrag()
	{
		mAxis = mHoveredAxis = -1;
	}

	bool TransformGizmo::CancelDrag(ECS::Transform* transform)
	{
		const bool restore = IsDragging() && transform;
		if (restore)
		{
			transform->SetPosition(mStartPosition);
			transform->SetRotation(mStartRotation);
			transform->SetScale(mStartScale);
			transform->Update();
		}
		EndDrag();
		return restore;
	}

	void TransformGizmo::Paint(UI::DrawList& list, UI::Rect clip, UI::Rect viewport,
							   const ECS::Transform& transform, const Components::CameraComponent& camera,
							   const Assets::Font* font, bool showHover) const
	{
		if (mMode == GizmoMode::Select)
			return;
		UI::Vec2 center;
		if (!Project(transform.GetLocalPosition(), camera, viewport, center) ||
			HandleLength(transform.GetLocalPosition(), camera, viewport) <= 0)
			return;
		clip = clip.Intersect(viewport);
		const int highlighted = IsDragging() ? mAxis : (showHover ? mHoveredAxis : -1);
		for (int axis = 0; axis < 3; ++axis)
		{
			const auto color = AxisColor(axis, axis == highlighted);
			const float thickness = axis == highlighted ? 3.f : 2.f;
			if (mMode == GizmoMode::Rotate)
			{
				for (int segment = 0; segment < RingSegments; ++segment)
				{
					UI::Vec2 start, end;
					const float angle = 2 * Pi * segment / RingSegments;
					if (GetHandlePosition(transform, camera, viewport, axis, start, angle) &&
						GetHandlePosition(transform, camera, viewport, axis, end,
										  angle + 2 * Pi / RingSegments))
						Line(list, clip, start, end, color, thickness);
				}
			}
			else
			{
				UI::Vec2 end;
				if (!GetHandlePosition(transform, camera, viewport, axis, end))
					continue;
				const float length = Distance(center, end);
				if (length < 14)
					continue;
				const UI::Vec2 unit = {(end.x - center.x) / length, (end.y - center.y) / length};
				Line(list, clip, center, end, color, thickness);
				if (mMode == GizmoMode::Scale)
					list.Add({end.x - 5, end.y - 5, 10, 10}, clip, color);
				else
				{
					Line(list, clip, end,
						 {end.x - unit.x * 11 - unit.y * 5, end.y - unit.y * 11 + unit.x * 5}, color,
						 thickness);
					Line(list, clip, end,
						 {end.x - unit.x * 11 + unit.y * 5, end.y - unit.y * 11 - unit.x * 5}, color,
						 thickness);
				}
				list.Text(font, std::string_view("XYZ").substr(axis, 1),
						  {end.x + unit.x * 15 - 7, end.y + unit.y * 15 - 8, 20, 20}, clip, 13, color,
						  UI::Alignment::Start, false);
			}
		}
		if (mMode != GizmoMode::Rotate)
		{
			list.Add({center.x - 6, center.y - 6, 12, 12}, clip,
					 highlighted == 3 ? UI::Color{1, .85f, .18f, 1} : UI::Color{.9f, .9f, .9f, 1});
			list.Add({center.x - 3, center.y - 3, 6, 6}, clip, {.04f, .055f, .075f, 1});
		}
	}
} // namespace Nuclear::Editor
