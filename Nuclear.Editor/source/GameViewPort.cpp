#include <GameViewPort.h>
#include <SkyboxComponent.h>
#include <Core/Scene.h>
#include <Components/CameraComponent.h>
#include <Components/EntityInfoComponent.h>
#include <Nuclear.Platform/Input.h>
#include <UI/UIContext.h>
#include <cmath>

namespace Nuclear::Editor
{
	Components::EntityInfoComponent* GameViewPort::SelectedInfo() const
	{
		auto& registry = Core::Scene::Get().GetRegistry();
		if (registry.valid(mSelected) && registry.all_of<SkyboxComponent>(mSelected))
			return nullptr;
		return registry.valid(mSelected) ? registry.try_get<Components::EntityInfoComponent>(mSelected)
										 : nullptr;
	}

	void GameViewPort::CancelTransform()
	{
		auto* info = SelectedInfo();
		if (mGizmo.CancelDrag(info ? &info->mTransform : nullptr) && OnTransformChanged)
			OnTransformChanged();
	}

	void GameViewPort::CancelInteraction()
	{
		if (GetContext())
			GetContext()->CancelSubtree(*this);
		CancelTransform();
		mDragging = false;
		mMovementKeys.fill(false);
	}

	void GameViewPort::SetSelectedEntity(entt::entity entity)
	{
		if (entity == mSelected)
			return;
		const bool focused = HasFocus();
		if (GetContext())
			GetContext()->CancelSubtree(*this);
		CancelTransform();
		mSelected = entity;
		if (focused && GetContext())
			GetContext()->SetFocus(GetId());
	}

	void GameViewPort::SetGizmoMode(GizmoMode mode)
	{
		if (mode == mGizmo.GetMode())
			return;
		if (GetContext())
			GetContext()->CancelSubtree(*this);
		CancelTransform();
		mGizmo.SetMode(mode);
	}

	void GameViewPort::UpdateCamera(float deltaTime)
	{
		auto* camera = Core::Scene::Get().GetMainCamera();
		if (!HasFocus() || !IsEnabled() || !IsVisible() || !camera)
		{
			mMovementKeys.fill(false);
			return;
		}
		if (mGizmo.IsDragging() || !std::isfinite(deltaTime) || deltaTime <= 0)
			return;

		const int forward = int(mMovementKeys[0]) - int(mMovementKeys[2]);
		const int right = int(mMovementKeys[3]) - int(mMovementKeys[1]);
		if (!forward && !right)
			return;

		const float step = forward && right ? deltaTime / std::sqrt(2.f) : deltaTime;
		if (forward)
			camera->ProcessMovement(forward > 0 ? Components::CAMERA_MOVEMENT_FORWARD
											  : Components::CAMERA_MOVEMENT_BACKWARD, step);
		if (right)
			camera->ProcessMovement(right > 0 ? Components::CAMERA_MOVEMENT_RIGHT
											: Components::CAMERA_MOVEMENT_LEFT, step);
		camera->UpdateBuffer();
	}

	void GameViewPort::OnArrange(UI::Rect content)
	{
		if (!(mDragging || mGizmo.IsDragging()))
			return;
		const bool changed = std::abs(content.x - mDragBounds.x) > .01f ||
							 std::abs(content.y - mDragBounds.y) > .01f ||
							 std::abs(content.Width - mDragBounds.Width) > .01f ||
							 std::abs(content.Height - mDragBounds.Height) > .01f;
		if (changed)
		{
			if (GetContext())
				GetContext()->CancelSubtree(*this);
			CancelTransform();
			mDragging = false;
		}
	}

	UI::EventReply GameViewPort::OnInput(const UI::InputEvent& event)
	{
		using Type = UI::InputEvent::Type;
		if (event.EventType == Type::Cancel)
		{
			mDragging = false;
			mMovementKeys.fill(false);
			CancelTransform();
			return {};
		}
		auto* camera = Core::Scene::Get().GetMainCamera();
		if (!camera)
			return {};
		auto* info = SelectedInfo();
		if (mGizmo.IsDragging() && !info)
		{
			mGizmo.CancelDrag(nullptr);
			return {true, false, true};
		}
		if (event.EventType == Type::KeyDown || event.EventType == Type::KeyUp)
		{
			constexpr int keys[] = {Platform::Input::KEYCODE_W, Platform::Input::KEYCODE_A,
								   Platform::Input::KEYCODE_S, Platform::Input::KEYCODE_D};
			for (size_t index = 0; index < mMovementKeys.size(); ++index)
			{
				if (event.Key != keys[index])
					continue;
				if (event.EventType == Type::KeyUp || !event.Repeat)
					mMovementKeys[index] = event.EventType == Type::KeyDown;
				return {true};
			}
		}
		if (event.EventType == Type::KeyDown && !event.Repeat)
		{
			if (event.Key == Platform::Input::KEYCODE_ESCAPE)
			{
				CancelTransform();
				mDragging = false;
				mMovementKeys.fill(false);
				return {true, false, true};
			}
			if (event.Key == Platform::Input::KEYCODE_Q || event.Key == Platform::Input::KEYCODE_G ||
				event.Key == Platform::Input::KEYCODE_E || event.Key == Platform::Input::KEYCODE_R)
			{
				SetGizmoMode(event.Key == Platform::Input::KEYCODE_Q   ? GizmoMode::Select
							 : event.Key == Platform::Input::KEYCODE_G ? GizmoMode::Move
							 : event.Key == Platform::Input::KEYCODE_E ? GizmoMode::Rotate
																	   : GizmoMode::Scale);
				return {true, false, true, true};
			}
		}
		if (event.EventType == Type::PointerDown)
		{
			if (mGizmo.IsDragging() || mDragging)
				return {true};
			if (event.Button == MOUSE_BUTTON_LEFT)
			{
				if (mGizmo.GetMode() == GizmoMode::Select)
				{
					if (OnSelectionRequested)
						OnSelectionRequested(event.Position);
					return {true, false, false, true};
				}
				const bool edit =
					info && mGizmo.BeginDrag(info->mTransform, *camera, GetBounds(), event.Position);
				if (edit)
					mDragStart = info->mTransform;
				mDragBounds = GetBounds();
				return {true, edit, false, true};
			}
			if (event.Button == MOUSE_BUTTON_RIGHT)
			{
				mDragging = true;
				mDragBounds = GetBounds();
				mLastPointer = event.Position;
				return {true, true, false, true};
			}
		}
		if (event.EventType == Type::PointerUp)
		{
			if (event.Button == MOUSE_BUTTON_LEFT && !mDragging)
			{
				if (mGizmo.IsDragging())
				{
					mGizmo.EndDrag();
					if (OnTransformCommitted)
						OnTransformCommitted(mSelected, mDragStart);
					if (OnTransformChanged)
						OnTransformChanged();
				}
				// Focus changes can cancel the edit before this release arrives.
				return {true, false, true};
			}
			if (event.Button == MOUSE_BUTTON_RIGHT && !mGizmo.IsDragging())
			{
				mDragging = false;
				return {true, false, true};
			}
		}
		if (event.EventType == Type::PointerMove)
		{
			if (mGizmo.IsDragging())
			{
				if (mGizmo.UpdateDrag(info->mTransform, *camera, GetBounds(), event.Position) &&
					OnTransformChanged)
					OnTransformChanged();
				return {true};
			}
			if (mDragging)
			{
				camera->ProcessEye((event.Position.x - mLastPointer.x) * 3,
								   (mLastPointer.y - event.Position.y) * 3);
				camera->UpdateBuffer();
				mLastPointer = event.Position;
				return {true};
			}
			if (info)
				mGizmo.UpdateHover(info->mTransform, *camera, GetBounds(), event.Position);
		}
		if (event.EventType == Type::Wheel)
		{
			if (!mGizmo.IsDragging() && !mDragging)
			{
				camera->SetPosition(camera->GetPosition() + camera->GetFrontView() * event.WheelY * .5f);
				camera->UpdateBuffer();
			}
			return {true};
		}
		return {};
	}

	void GameViewPort::OnPaint(UI::DrawList& list, UI::Rect clip) const
	{
		auto* camera = Core::Scene::Get().GetMainCamera();
		list.Add(GetBounds(), clip, {1, 1, 1, 1}, camera ? camera->GetColorRT().GetSRV() : nullptr);
		if (auto* info = SelectedInfo(); info && camera)
			mGizmo.Paint(list, clip, GetBounds(), info->mTransform, *camera, ResolveFont(nullptr),
						 IsHovered());
	}
} // namespace Nuclear::Editor
