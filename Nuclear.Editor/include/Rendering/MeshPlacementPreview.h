#pragma once
#include <Assets/Mesh.h>
#include <Components/CameraComponent.h>
#include <UI/Types.h>
#include <Diligent/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <Diligent/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <optional>

namespace Nuclear::Editor
{
	/// Transient mesh placement without authored ECS entities. GPU work runs only from Update/Render.
	class MeshPlacementPreview
	{
	public:
		void Begin(const Core::UUID& mesh); ///< Stores a stable identity; changing mesh resets cached contact bounds.
		void Cancel(); ///< Clears placement validity and identity without changing scene/history.
		/// Resolves a ready mesh and rests its bounds against the scene surface, ground Y=0, or ten units along the ray.
		/// viewport/point use logical target-context coordinates. Returns false outside the viewport or on invalid rays.
		bool Update(Components::CameraComponent& camera, UI::Rect viewport, UI::Vec2 point);
		void Render(Components::CameraComponent& camera); ///< Draws a translucent ghost without writing scene depth.
		const std::optional<Core::UUID>& GetMesh() const { return mMesh; } ///< Pending asset identity, or empty.
		Math::Vector3 GetPosition() const { return mPosition; } ///< Last valid placement pivot in world units.
		bool IsValid() const { return mValid; } ///< True after a successful Update for the current asset.
	private:
		bool InitializePipeline(Components::CameraComponent& camera); ///< Matches the scene camera target formats.
		float ReadDepth(Components::CameraComponent& camera, UI::Rect viewport, UI::Vec2 point); ///< Reads scene depth; 1 means background/unavailable.
		void MeasureBounds(Assets::Mesh& mesh); ///< Caches contact bounds from CPU data or uploaded vertex buffers.
		/// Private GPU constants shared by ghost shaders.
		struct Constants
		{
			Math::Matrix4 ModelViewProjection; ///< Camera projection/view and translated mesh transform.
			Math::Vector4 LightDirection; ///< Unit direction toward the demo light.
			Math::Vector4 BaseColor; ///< Linear ghost tint with alpha.
		};
		std::optional<Core::UUID> mMesh; ///< Stable pending library identity; resources remain library-owned.
		Math::Vector3 mPosition{0}; ///< World pivot of the pending placement.
		Math::Vector3 mMinimum{0}; ///< Minimum finite local vertex coordinates for contact support.
		Math::Vector3 mMaximum{0}; ///< Maximum finite local vertex coordinates for contact support.
		Math::Vector3 mHitNormal{0, 1, 0}; ///< Surface normal facing the camera, or ground up when no depth surface exists.
		bool mMeasured = false; ///< Contact bounds have been read for this mesh.
		bool mValid = false; ///< Last target/ray resolved to a usable position.
		Diligent::RefCntAutoPtr<Diligent::IPipelineState> mPipeline; ///< Alpha-blended ghost pipeline with depth writes disabled.
		Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> mBinding; ///< Private shader resources.
		Diligent::RefCntAutoPtr<Diligent::IBuffer> mConstantBuffer; ///< Ghost transform, light and color.
		Diligent::RefCntAutoPtr<Diligent::ITexture> mDepthReadback; ///< Staging depth target; resized with the scene camera.
	};
}
