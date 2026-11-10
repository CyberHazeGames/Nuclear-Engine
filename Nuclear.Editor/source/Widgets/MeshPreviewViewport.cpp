#include <MeshPreviewViewport.h>
#include <Assets/AssetLibrary.h>
#include <Nuclear.Platform/Input.h>
#include <UI/UIContext.h>
#include <UI/DrawList.h>
#include <cmath>

namespace Nuclear::Editor
{
	MeshPreviewViewport::MeshPreviewViewport(const Core::UUID& mesh) : mMesh(mesh)
	{
		SetSizeLimits({160, 180}, {1000000, 1000000});
	}

	MeshPreviewViewport::MeshPreviewViewport(Assets::AssetType type, const Core::UUID& asset)
		: MeshPreviewViewport(asset)
	{
		mType = type;
	}

	void MeshPreviewViewport::CancelNavigation()
	{
		mKeys.fill(false);
		mLooking = false;
	}

	void MeshPreviewViewport::FrameMesh()
	{
		if (GetContext()) GetContext()->CancelSubtree(*this);
		CancelNavigation();
		mScene.FrameMesh();
	}

	void MeshPreviewViewport::Update(float deltaTime)
	{
		if (!HasFocus() || !IsVisible() || !IsEnabled())
		{
			CancelNavigation();
			return;
		}
		if (!std::isfinite(deltaTime) || deltaTime <= 0) return;
		auto& camera = mScene.GetCamera();
		const int forward = int(mKeys[0]) - int(mKeys[2]);
		const int right = int(mKeys[3]) - int(mKeys[1]);
		const float step = forward && right ? deltaTime / std::sqrt(2.f) : deltaTime;
		if (forward) camera.ProcessMovement(forward > 0 ? Components::CAMERA_MOVEMENT_FORWARD : Components::CAMERA_MOVEMENT_BACKWARD, step);
		if (right) camera.ProcessMovement(right > 0 ? Components::CAMERA_MOVEMENT_RIGHT : Components::CAMERA_MOVEMENT_LEFT, step);
		camera.UpdateBuffer();
	}

	void MeshPreviewViewport::Render()
	{
		if (!IsVisible() || !GetContext()) return;
		auto& library = Assets::AssetLibrary::Get();
		auto* material = mType == Assets::AssetType::Material ? library.mImportedMaterials.GetAsset(mMesh) : nullptr;
		auto* mesh = mType == Assets::AssetType::Material ? Assets::DefaultMeshes::GetSphereAsset() : library.mImportedMeshes.GetAsset(mMesh);
		const auto bounds = GetBounds();
		if (!mesh || bounds.Width <= 0 || bounds.Height <= 0) return;
		if (mType == Assets::AssetType::Material && !material) return;
		const float scale = GetContext()->GetScale();
		if (mScene.Render(*mesh, Uint32(std::clamp(bounds.Width * scale, 1.f, 4096.f)),
			Uint32(std::clamp(bounds.Height * scale, 1.f, 4096.f)), material, mTextureSet))
			mView = mScene.GetTexture().GetTextureView();
	}

	void MeshPreviewViewport::OnArrange(UI::Rect bounds)
	{
		if (mLooking && (bounds.x != mDragBounds.x || bounds.y != mDragBounds.y ||
			bounds.Width != mDragBounds.Width || bounds.Height != mDragBounds.Height))
		{
			if (GetContext()) GetContext()->CancelSubtree(*this);
			CancelNavigation();
		}
	}

	UI::EventReply MeshPreviewViewport::OnInput(const UI::InputEvent& event)
	{
		using Type = UI::InputEvent::Type;
		using Key = Platform::Input;
		auto& camera = mScene.GetCamera();
		if (event.EventType == Type::Cancel)
		{
			CancelNavigation();
			return {true, false, true};
		}
		if (event.EventType == Type::KeyDown || event.EventType == Type::KeyUp)
		{
			constexpr int keys[] = {Key::KEYCODE_W, Key::KEYCODE_A, Key::KEYCODE_S, Key::KEYCODE_D};
			for (size_t i = 0; i < mKeys.size(); ++i)
				if (event.Key == keys[i])
				{
					mKeys[i] = event.EventType == Type::KeyDown;
					return {true};
				}
			if (event.EventType == Type::KeyDown && event.Key == Key::KEYCODE_ESCAPE)
			{
				CancelNavigation();
				return {true, false, true};
			}
			if (event.EventType == Type::KeyDown && event.Key == Key::KEYCODE_F && !event.Repeat)
			{
				FrameMesh();
				return {true};
			}
		}
		if (event.EventType == Type::PointerDown)
		{
			if (event.Button == MOUSE_BUTTON_RIGHT)
			{
				mLooking = true;
				mLastPointer = event.Position;
				mDragBounds = GetBounds();
				return {true, true, false, true};
			}
			if (event.Button == MOUSE_BUTTON_LEFT) return {true, false, false, true};
		}
		if (event.EventType == Type::PointerUp && event.Button == MOUSE_BUTTON_RIGHT)
		{
			mLooking = false;
			return {true, false, true};
		}
		if (event.EventType == Type::PointerMove && mLooking)
		{
			camera.ProcessEye((event.Position.x - mLastPointer.x) * 3, (mLastPointer.y - event.Position.y) * 3);
			camera.UpdateBuffer();
			mLastPointer = event.Position;
			return {true};
		}
		if (event.EventType == Type::Wheel)
		{
			camera.SetPosition(camera.GetPosition() + camera.GetFrontView() * event.WheelY * camera.MovementSpeed * .2f);
			camera.UpdateBuffer();
			return {true};
		}
		return {};
	}

	void MeshPreviewViewport::OnPaint(UI::DrawList& list, UI::Rect clip) const
	{
		// The wrapper is stable across resize; painting never allocates GPU resources.
		list.Add(GetBounds(), clip, {.055f, .065f, .08f, 1});
		if (mView) list.Add(GetBounds(), clip, {1, 1, 1, 1}, mView);
	}
}
