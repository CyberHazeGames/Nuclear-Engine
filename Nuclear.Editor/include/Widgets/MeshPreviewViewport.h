#pragma once
#include <UI/Widget.h>
#include <MeshPreviewScene.h>
#include <array>

namespace Nuclear::Editor
{
	/// Document-owned flyable mesh or material-sphere preview; resources and input survive tab switches and docking.
	class MeshPreviewViewport : public UI::Widget
	{
	public:
		explicit MeshPreviewViewport(const Core::UUID& mesh); ///< Stores a library UUID, never takes asset ownership.
		/// Stores a Mesh or Material library identity. Materials render on the built-in sphere.
		MeshPreviewViewport(Assets::AssetType type, const Core::UUID& asset);
		void SetTextureSet(Uint32 index) { mTextureSet = index; } ///< Chooses a material's source texture set; index zero is the default.
		void Update(float deltaTime); ///< Advances focused camera movement in seconds; hidden/disabled views stop movement.
		void Render(); ///< Renders visible ready geometry outside retained painting/input, using the owning context's DPI.
		void FrameMesh(); ///< Restores the preview camera and clears pending movement.
		MeshPreviewScene& GetScene() { return mScene; } ///< Borrowed private preview resources for inspection.
		bool IsFocusable() const override { return true; } ///< Receives camera navigation keys.
		UI::EventReply OnInput(const UI::InputEvent& event) override; ///< Right-drag looks; WASD flies; wheel travels; F frames; Escape cancels.
	protected:
		void OnPaint(UI::DrawList& list, UI::Rect clip) const override; ///< Samples the last completed preview image.
		void OnArrange(UI::Rect bounds) override; ///< Cancels camera capture if geometry changes during a drag.
	private:
		void CancelNavigation(); ///< Clears held movement and pointer-look state.
		Core::UUID mMesh; ///< Stable asset identity resolved for each render.
		Assets::AssetType mType = Assets::AssetType::Mesh; ///< Mesh geometry or material sphere preview.
		Uint32 mTextureSet = 0; ///< Source material texture set shown on the sphere.
		MeshPreviewScene mScene; ///< Owns the private camera, sky and render resources.
		Diligent::RefCntAutoPtr<Diligent::ITextureView> mView; ///< Last completed output, retained across target recreation.
		std::array<bool, 4> mKeys{}; ///< Held W/A/S/D inputs for this focused surface.
		bool mLooking = false; ///< Right-button camera gesture is captured.
		UI::Vec2 mLastPointer; ///< Previous pointer position in logical units.
		UI::Rect mDragBounds; ///< Bounds when look capture began.
	};
}
