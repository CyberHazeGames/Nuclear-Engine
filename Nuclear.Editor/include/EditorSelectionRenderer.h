#pragma once

#include <ECS/entt/entity/entity.hpp>
#include <UI/Types.h>
#include <memory>

namespace Nuclear::Components { class CameraComponent; }

namespace Nuclear::Editor
{
	/// Editor-owned mesh ID buffer and visible-silhouette outline. Uses existing GPU
	/// mesh buffers, including imported meshes whose CPU geometry has been released.
	/// All methods run on the graphics thread; scene assets and cameras are borrowed.
	class EditorSelectionRenderer
	{
	public:
		EditorSelectionRenderer(); ///< Creates empty CPU-side resource ownership.
		~EditorSelectionRenderer(); ///< Releases owned GPU resources before graphics shutdown.
		/// Builds pipelines for the camera's color format; false on shader/GPU creation failure.
		bool Initialize(Components::CameraComponent& camera);
		void Shutdown(); ///< Releases pipelines, targets, readback and temporary entity identities.
		/// Renders a two-logical-unit yellow outline around the selected visible mesh.
		/// Preserves scene colors/depth; null, deleted, hidden and occluded selections draw nothing.
		/// Returns false on resource failure. The caller restores its next render target/viewport.
		bool Render(Components::CameraComponent& camera, entt::entity selected, float scale);
		/// Renders current visible mesh IDs and reads one pixel at a logical viewport position.
		/// Returns the nearest live mesh identity, or null for background/outside/failure.
		/// Performs a synchronous GPU readback only for this explicit click operation.
		entt::entity Pick(Components::CameraComponent& camera, UI::Rect viewport, UI::Vec2 position);

	private:
		bool RenderIds(Components::CameraComponent& camera); ///< Refreshes depth-tested IDs at camera-target resolution.
		bool Resize(unsigned width, unsigned height); ///< Recreates ID/depth targets when physical dimensions change.
		struct Resources; ///< Private pipelines, GPU targets and per-render entity-ID mapping.
		std::unique_ptr<Resources> pResources; ///< Exclusive GPU ownership; never owns scene assets.
	};
}
