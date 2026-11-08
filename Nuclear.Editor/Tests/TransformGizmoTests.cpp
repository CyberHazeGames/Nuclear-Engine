#include "TransformGizmoTests.h"
#include <TransformGizmo.h>
#include <GameViewPort.h>
#include <Components/CameraComponent.h>
#include <Components/EntityInfoComponent.h>
#include <Core/Scene.h>
#include <UI/UIContext.h>
#include <UI/CanvasPanel.h>
#include <UI/Button.h>
#include <cmath>
#include <stdexcept>

namespace Nuclear::Editor::Tests
{
	namespace
	{
		void Check(bool condition, const char* message)
		{
			if (!condition)
				throw std::runtime_error(message);
		}

		bool Near(float left, float right)
		{
			return std::abs(left - right) < .002f;
		}

		ECS::Transform Identity()
		{
			return ECS::Transform(Math::Vector3(0), Math::Vector3(1), Math::Quaternion(1, 0, 0, 0));
		}

		UI::Vec2 Handle(TransformGizmo& gizmo, ECS::Transform& transform, Components::CameraComponent& camera,
						UI::Rect viewport, int axis, float angle = .7f)
		{
			UI::Vec2 point;
			Check(gizmo.GetHandlePosition(transform, camera, viewport, axis, point, angle),
				  "Gizmo handle projects into viewport");
			return point;
		}

		UI::Vec2 Extend(UI::Vec2 center, UI::Vec2 end, float amount)
		{
			return {end.x + (end.x - center.x) * amount, end.y + (end.y - center.y) * amount};
		}

		void Pointer(UI::UIContext& context, Platform::EventType type, UI::Vec2 point, bool down = false,
					 int button = MOUSE_BUTTON_LEFT)
		{
			Platform::Event event;
			event.mType = type;
			event.mMouseX = static_cast<int>(std::lround(point.x * context.GetScale()));
			event.mMouseY = static_cast<int>(std::lround(point.y * context.GetScale()));
			event.mPressed = down;
			event.mButton = button;
			context.ProcessEvent(event);
		}

		void Key(UI::UIContext& context, Platform::Input::KeyCode key, bool down = true)
		{
			Platform::Event event;
			event.mType = Platform::EventType::Key;
			event.mKey = key;
			event.mPressed = down;
			context.ProcessEvent(event);
		}
	} // namespace

	void RunTransformGizmoTests()
	{
		Components::CameraComponentDesc cameraDesc;
		cameraDesc.mInitRTs = false;
		cameraDesc.mProjection = Math::perspective(Math::radians(45.f), 800.f / 600, .1f, 1000.f);
		cameraDesc.mYaw = -116.565f;
		cameraDesc.mPitch = -16.602f;
		Components::CameraComponent camera(cameraDesc);
		camera.SetPosition({3, 2, 6});
		camera.UpdateBuffer();
		const UI::Rect viewport = {40, 20, 800, 600};
		TransformGizmo gizmo;
		gizmo.SetMode(GizmoMode::Select);
		auto selectedTransform = Identity();
		UI::Vec2 hiddenHandle;
		Check(!gizmo.GetHandlePosition(selectedTransform, camera, viewport, 0, hiddenHandle),
			  "Select mode has no transform handle coordinates");
		Check(!gizmo.BeginDrag(selectedTransform, camera, viewport, {440, 320}),
			  "Select mode cannot begin a transform drag");
		UI::DrawList selectionDrawing;
		gizmo.Paint(selectionDrawing, viewport, viewport, selectedTransform, camera, nullptr);
		Check(selectionDrawing.Quads.empty(), "Select mode draws no transform handles");

		for (int axis = 0; axis < 3; ++axis)
		{
			auto transform = Identity();
			gizmo.SetMode(GizmoMode::Move);
			auto center = Handle(gizmo, transform, camera, viewport, 3);
			auto start = Handle(gizmo, transform, camera, viewport, axis);
			Check(gizmo.BeginDrag(transform, camera, viewport, start), "Move axis can be captured");
			Check(gizmo.UpdateDrag(transform, camera, viewport, Extend(center, start, .4f)),
				  "Move drag applies");
			auto position = transform.GetLocalPosition();
			Check(position[axis] > .1f, "Move follows the positive world axis");
			for (int other = 0; other < 3; ++other)
				if (other != axis)
					Check(Near(position[other], 0), "Move leaves other coordinates unchanged");
			Check(gizmo.CancelDrag(&transform) && transform.GetLocalPosition() == Math::Vector3(0),
				  "Move cancel restores snapshot");

			gizmo.SetMode(GizmoMode::Scale);
			center = Handle(gizmo, transform, camera, viewport, 3);
			start = Handle(gizmo, transform, camera, viewport, axis);
			Check(gizmo.BeginDrag(transform, camera, viewport, start), "Scale axis can be captured");
			gizmo.UpdateDrag(transform, camera, viewport, Extend(center, start, .4f));
			auto scale = transform.GetLocalScale();
			Check(scale[axis] > 1.1f, "Scale grows the dragged dimension");
			for (int other = 0; other < 3; ++other)
				if (other != axis)
					Check(Near(scale[other], 1), "Scale leaves other dimensions unchanged");
			gizmo.CancelDrag(&transform);

			gizmo.SetMode(GizmoMode::Rotate);
			start = Handle(gizmo, transform, camera, viewport, axis);
			auto end = Handle(gizmo, transform, camera, viewport, axis, 1.1f);
			Check(gizmo.BeginDrag(transform, camera, viewport, start), "Rotation ring can be captured");
			Check(gizmo.UpdateDrag(transform, camera, viewport, end), "Rotation drag applies");
			Math::Vector3 rotationAxis(0);
			rotationAxis[axis] = 1;
			const auto expected = Math::angleAxis(.4f, rotationAxis);
			Check(std::abs(Math::dot(transform.GetLocalRotation(), expected)) > .999f,
				  "Rotation follows its world-axis ring in radians");
			gizmo.CancelDrag(&transform);
			Check(transform.GetLocalRotation() == Math::Quaternion(1, 0, 0, 0),
				  "Rotation cancel restores original quaternion");
		}

		auto transform = Identity();
		gizmo.SetMode(GizmoMode::Scale);
		auto center = Handle(gizmo, transform, camera, viewport, 3);
		Check(gizmo.BeginDrag(transform, camera, viewport, center), "Uniform scale center can be captured");
		gizmo.UpdateDrag(transform, camera, viewport, {center.x + 30, center.y - 20});
		const auto uniform = transform.GetLocalScale();
		Check(uniform.x > 1 && Near(uniform.x, uniform.y) && Near(uniform.y, uniform.z),
			  "Center scales all dimensions equally");
		gizmo.EndDrag();
		Check(!gizmo.IsDragging() && transform.GetLocalScale() == uniform,
			  "Release commits the current transform");
		transform = Identity();
		transform.SetScale({-2, 1, 1});
		auto start = Handle(gizmo, transform, camera, viewport, 0);
		gizmo.BeginDrag(transform, camera, viewport, start);
		gizmo.UpdateDrag(transform, camera, viewport, Extend(center, start, 1));
		Check(transform.GetLocalScale().x < -2, "Scaling preserves mirrored signs");
		gizmo.CancelDrag(&transform);
		// Existing default Transform construction uses a zero quaternion, which still renders as identity.
		ECS::Transform defaultTransform;
		gizmo.SetMode(GizmoMode::Rotate);
		start = Handle(gizmo, defaultTransform, camera, viewport, 0);
		auto rotationEnd = Handle(gizmo, defaultTransform, camera, viewport, 0, 1.1f);
		Check(gizmo.BeginDrag(defaultTransform, camera, viewport, start),
			  "Default transform can start rotation");
		gizmo.UpdateDrag(defaultTransform, camera, viewport, rotationEnd);
		Check(Near(Math::dot(defaultTransform.GetLocalRotation(), defaultTransform.GetLocalRotation()), 1),
			  "Default zero quaternion becomes a valid orientation");
		gizmo.CancelDrag(&defaultTransform);
		Check(defaultTransform.GetLocalRotation() == Math::Quaternion(0, 0, 0, 0),
			  "Cancellation preserves even a zero starting quaternion");

		transform = Identity();
		gizmo.SetMode(GizmoMode::Move);
		center = Handle(gizmo, transform, camera, viewport, 3);
		gizmo.BeginDrag(transform, camera, viewport, center);
		gizmo.UpdateDrag(transform, camera, viewport, {center.x + 30, center.y - 20});
		const auto displacement = transform.GetLocalPosition();
		Check(Math::length(displacement) > .1f &&
				  std::abs(Math::dot(displacement, camera.GetFrontView())) < .001f,
			  "Center move remains in the camera-facing plane");
		gizmo.CancelDrag(&transform);

		// A front-facing camera sees the X rotation ring edge-on; tangent dragging stays finite.
		Components::CameraComponentDesc frontDesc = cameraDesc;
		frontDesc.mYaw = -90;
		frontDesc.mPitch = 0;
		Components::CameraComponent front(frontDesc);
		front.SetPosition({0, 0, 6});
		front.UpdateBuffer();
		gizmo.SetMode(GizmoMode::Rotate);
		start = Handle(gizmo, transform, front, viewport, 0, .4f);
		Check(gizmo.BeginDrag(transform, front, viewport, start),
			  "Edge-on rotation ring has a stable fallback");
		Check(gizmo.UpdateDrag(transform, front, viewport, {start.x, start.y + 25}),
			  "Edge-on rotation drag applies");
		Check(std::isfinite(transform.GetLocalRotation().x) &&
				  std::abs(transform.GetLocalRotation().x) > .01f,
			  "Edge-on rotation is finite and nonzero");
		gizmo.CancelDrag(&transform);
		transform.SetPosition(camera.GetPosition() - camera.GetFrontView() * 4.f);
		Check(!gizmo.GetHandlePosition(transform, camera, viewport, 0, start),
			  "Gizmos behind the camera are not interactive");

		// Exercise the retained input path at 2x scale, including cancellation and stale ECS handles.
		auto& scene = Core::Scene::Get();
		auto* previousCamera = scene.GetMainCamera();
		scene.SetMainCamera(&camera);
		auto entity = scene.CreateEntity("Gizmo input test", Identity());
		const auto entityId = entity.GetEntityID();
		UI::UIContext context;
		context.SetViewport(1600, 1200, 2);
		auto surface = std::make_unique<GameViewPort>();
		auto* view = surface.get();
		view->SetSelectedEntity(entityId);
		int changes = 0;
		view->OnTransformChanged = [&changes]() {
			++changes;
		};
		context.AddToViewport(std::move(surface));
		context.Layout();
		auto& target = entity.GetComponent<Components::EntityInfoComponent>().mTransform;
		gizmo.SetMode(GizmoMode::Move);
		auto begin = [&]() {
			center = Handle(gizmo, target, camera, view->GetBounds(), 3);
			start = Handle(gizmo, target, camera, view->GetBounds(), 0);
			Pointer(context, Platform::EventType::MouseButton, start, true);
			Pointer(context, Platform::EventType::MouseMove, Extend(center, start, .4f));
			Check(context.GetCapturedWidget() == view->GetId() && target.GetLocalPosition().x > .1f,
				  "Viewport captures scaled handle input");
		};
		begin();
		Pointer(context, Platform::EventType::MouseButton, start, false, MOUSE_BUTTON_RIGHT);
		Check(context.GetCapturedWidget() == view->GetId(),
			  "An unrelated button release does not commit the gizmo");
		Platform::Event escape;
		escape.mType = Platform::EventType::Key;
		escape.mKey = Platform::Input::KEYCODE_ESCAPE;
		escape.mPressed = true;
		context.ProcessEvent(escape);
		Check(!context.GetCapturedWidget() && target.GetLocalPosition() == Math::Vector3(0),
			  "Escape rolls back and releases capture");
		begin();
		Platform::Event focus;
		focus.mType = Platform::EventType::FocusChanged;
		focus.mFocused = false;
		context.ProcessEvent(focus);
		Check(!context.GetCapturedWidget() && target.GetLocalPosition() == Math::Vector3(0),
			  "Focus loss rolls back the drag");
		focus.mFocused = true;
		context.ProcessEvent(focus);
		auto secondRoot = std::make_unique<UI::CanvasPanel>();
		secondRoot->Add<UI::Button>();
		const auto secondRootId = secondRoot->GetId();
		context.AddToViewport(std::move(secondRoot), 1);
		begin();
		Platform::Event tab;
		tab.mType = Platform::EventType::Key;
		tab.mKey = Platform::Input::KEYCODE_TAB;
		tab.mPressed = true;
		context.ProcessEvent(tab);
		Check(target.GetLocalPosition() == Math::Vector3(0), "Tabbing away rolls back the pending edit");
		Pointer(context, Platform::EventType::MouseButton, start);
		Check(!context.GetCapturedWidget(), "Release after focus cancellation cannot leave capture stuck");
		context.RemoveFromViewport(secondRootId);
		begin();
		context.SetViewport(1200, 800, 2);
		context.Layout();
		Check(!context.GetCapturedWidget() && target.GetLocalPosition() == Math::Vector3(0),
			  "Resize cancels without a transform jump");
		context.SetViewport(1600, 1200, 2);
		context.Layout();

		const auto cameraStart = camera.GetPosition();
		begin();
		Key(context, Platform::Input::KEYCODE_W);
		view->UpdateCamera(.2f);
		Check(camera.GetPosition() == cameraStart, "WASD cannot move the camera during a transform drag");
		Key(context, Platform::Input::KEYCODE_Q);
		Check(!context.GetCapturedWidget() && target.GetLocalPosition() == Math::Vector3(0),
			  "Select mode cancels and restores a pending transform edit");
		Pointer(context, Platform::EventType::MouseButton, start, true);
		Pointer(context, Platform::EventType::MouseMove, Extend(center, start, .4f));
		Pointer(context, Platform::EventType::MouseButton, start);
		Check(!context.GetCapturedWidget() && target.GetLocalPosition() == Math::Vector3(0),
			  "Select mode ignores old handle locations without clearing the selected entity");

		using Input = Platform::Input;
		const auto screenRight = Math::Vector3(Math::inverse(camera.mCameraData.View)[0]);
		auto focusViewport = [&]() {
			const auto bounds = view->GetBounds();
			const UI::Vec2 empty = {bounds.x + 20, bounds.y + 20};
			Pointer(context, Platform::EventType::MouseButton, empty, true);
			Pointer(context, Platform::EventType::MouseButton, empty);
		};
		const Input::KeyCode directions[] = {Input::KEYCODE_W, Input::KEYCODE_A,
											   Input::KEYCODE_S, Input::KEYCODE_D};
		const Math::Vector3 expected[] = {camera.GetFrontView(), -screenRight,
											-camera.GetFrontView(), screenRight};
		for (int index = 0; index < 4; ++index)
		{
			camera.SetPosition(cameraStart);
			focusViewport();
			Key(context, directions[index]);
			view->UpdateCamera(.2f);
			Check(Math::length(camera.GetPosition() - cameraStart - expected[index] * .5f) < .002f,
				  "WASD moves along the camera forward/backward and screen-left/right directions");
			const auto stopped = camera.GetPosition();
			Key(context, directions[index], false);
			view->UpdateCamera(.2f);
			Check(camera.GetPosition() == stopped, "Key release stops continuous camera movement");
		}
		camera.SetPosition(cameraStart);
		Key(context, Input::KEYCODE_W);
		view->UpdateCamera(.1f);
		view->UpdateCamera(.1f);
		Check(Math::length(camera.GetPosition() - cameraStart - camera.GetFrontView() * .5f) < .002f,
			  "Held camera movement is independent of frame subdivision");
		camera.SetPosition(cameraStart);
		Key(context, Input::KEYCODE_D);
		view->UpdateCamera(.2f);
		Check(Near(Math::length(camera.GetPosition() - cameraStart), .5f),
			  "Diagonal camera movement keeps the configured speed");
		Key(context, Input::KEYCODE_W, false);
		Key(context, Input::KEYCODE_D, false);
		camera.SetPosition(cameraStart);
		Key(context, Input::KEYCODE_W);
		Key(context, Input::KEYCODE_S);
		view->UpdateCamera(.2f);
		Check(camera.GetPosition() == cameraStart, "Opposite movement keys cancel each other");
		Key(context, Input::KEYCODE_S, false);
		context.ResetInput();
		view->UpdateCamera(.2f);
		focusViewport();
		view->UpdateCamera(.2f);
		Check(camera.GetPosition() == cameraStart, "Losing and restoring viewport focus clears held keys");
		Key(context, Input::KEYCODE_D);
		focus.mFocused = false;
		context.ProcessEvent(focus);
		view->UpdateCamera(.2f);
		focus.mFocused = true;
		context.ProcessEvent(focus);
		focusViewport();
		view->UpdateCamera(.2f);
		Check(camera.GetPosition() == cameraStart, "Window focus loss prevents stuck camera movement");
		camera.UpdateBuffer();
		view->SetGizmoMode(GizmoMode::Move);
		begin();
		scene.GetRegistry().destroy(entityId);
		Pointer(context, Platform::EventType::MouseMove, start);
		Check(!context.GetCapturedWidget(), "Deleted drag targets release capture without a stale pointer");
		Check(changes >= 4, "Drag updates and rollback notify the inspector");
		context.Clear();
		scene.SetMainCamera(previousCamera);
	}
} // namespace Nuclear::Editor::Tests
